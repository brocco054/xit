#pragma once

#include <d3d11.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <algorithm>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "ole32.lib")

namespace acervo {
enum class preview_state { pending, ready, missing };

struct preview_texture {
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> view;
	UINT width = 0;
	UINT height = 0;
	int last_frame = 0;
	preview_state state = preview_state::pending;

	size_t bytes() const { return view ? size_t(width) * height * 4 : 0; }
};

inline std::filesystem::path preview_directory() {
	static const int anchor = 0;
	HMODULE module = nullptr;
	if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
		reinterpret_cast<LPCWSTR>(&anchor), &module))
		return {};
	wchar_t path[32768];
	const DWORD length = GetModuleFileNameW(module, path, _countof(path));
	if (length == 0 || length >= _countof(path))
		return {};
	const auto dll_dir = std::filesystem::path(path).parent_path();
	const auto local_assets = dll_dir / L"assets" / L"preview";
	std::error_code error;
	if (std::filesystem::is_directory(local_assets, error))
		return local_assets;
	return dll_dir.parent_path().parent_path() / L"assets" / L"preview";
}

struct preview_pixels {
	UINT width = 0;
	UINT height = 0;
	std::vector<BYTE> pixels;
};

inline preview_pixels decode_preview(IWICImagingFactory* factory, const std::filesystem::path& path) {
	using Microsoft::WRL::ComPtr;
	ComPtr<IWICBitmapDecoder> decoder;
	ComPtr<IWICBitmapFrameDecode> frame;
	ComPtr<IWICFormatConverter> converter;
	preview_pixels result;
	if (!factory
		|| FAILED(factory->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder))
		|| FAILED(decoder->GetFrame(0, &frame))
		|| FAILED(frame->GetSize(&result.width, &result.height))
		|| result.width == 0 || result.height == 0 || result.width > 1024 || result.height > 1024
		|| FAILED(factory->CreateFormatConverter(&converter))
		|| FAILED(converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone,
			nullptr, 0.0, WICBitmapPaletteTypeCustom)))
		return {};

	const UINT stride = result.width * 4;
	result.pixels.resize(stride * result.height);
	if (FAILED(converter->CopyPixels(nullptr, stride, static_cast<UINT>(result.pixels.size()), result.pixels.data())))
		return {};
	return result;
}

inline preview_texture upload_preview(ID3D11Device* device, const preview_pixels& pixels) {
	preview_texture result;
	result.state = preview_state::missing;
	if (!device || pixels.pixels.empty())
		return result;

	D3D11_TEXTURE2D_DESC description{};
	description.Width = pixels.width;
	description.Height = pixels.height;
	description.MipLevels = 1;
	description.ArraySize = 1;
	description.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	description.SampleDesc.Count = 1;
	description.Usage = D3D11_USAGE_IMMUTABLE;
	description.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	D3D11_SUBRESOURCE_DATA data{ pixels.pixels.data(), pixels.width * 4, 0 };
	Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
	if (FAILED(device->CreateTexture2D(&description, &data, &texture))
		|| FAILED(device->CreateShaderResourceView(texture.Get(), nullptr, &result.view)))
		return result;
	result.width = pixels.width;
	result.height = pixels.height;
	result.state = preview_state::ready;
	return result;
}

class preview_cache {
	using key_type = std::uint64_t;
	static constexpr size_t texture_budget = 32 * 1024 * 1024;
	static constexpr size_t max_pending = 16;
	static constexpr size_t max_uploads_per_frame = 2;

	// Texture ownership and eviction stay on the render thread.
	std::unordered_map<key_type, preview_texture> m_textures;
	ID3D11Device* m_device = nullptr;
	int m_frame = -1;
	size_t m_bytes = 0;
	std::mutex m_mutex;
	std::condition_variable m_wake;
	std::deque<key_type> m_jobs;
	std::deque<std::pair<key_type, preview_pixels>> m_ready;
	size_t m_outstanding = 0;
	bool m_stop = false;
	std::thread m_worker;

	void decode() {
		const HRESULT apartment = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
		Microsoft::WRL::ComPtr<IWICImagingFactory> factory;
		if (SUCCEEDED(apartment))
			CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
		const auto directory = preview_directory();
		for (;;) {
			key_type key;
			{
				std::unique_lock lock(m_mutex);
				m_wake.wait(lock, [&] { return m_stop || !m_jobs.empty(); });
				if (m_stop)
					break;
				key = m_jobs.front();
				m_jobs.pop_front();
			}
			const auto filename = std::to_wstring(key >> 32) + L"-" +
				std::to_wstring(static_cast<std::uint32_t>(key)) + L".png";
			auto pixels = directory.empty() ? preview_pixels{} : decode_preview(factory.Get(), directory / filename);
			std::lock_guard lock(m_mutex);
			m_ready.emplace_back(key, std::move(pixels));
		}
		factory.Reset();
		if (SUCCEEDED(apartment))
			CoUninitialize();
	}

	void begin_frame(ID3D11Device* device, int frame) {
		if (frame == m_frame && device == m_device)
			return;
		m_frame = frame;
		if (device != m_device) {
			for (auto it = m_textures.begin(); it != m_textures.end();) {
				if (it->second.state == preview_state::pending)
					++it;
				else
					it = m_textures.erase(it);
			}
			m_bytes = 0;
			m_device = device;
		}

		for (size_t upload = 0; device && upload < max_uploads_per_frame; ++upload) {
			std::pair<key_type, preview_pixels> ready;
			{
				std::lock_guard lock(m_mutex);
				if (m_ready.empty())
					break;
				ready = std::move(m_ready.front());
				m_ready.pop_front();
				--m_outstanding;
			}
			auto& entry = m_textures.at(ready.first);
			const int requested_frame = entry.last_frame;
			entry = upload_preview(device, ready.second);
			entry.last_frame = requested_frame;
			m_bytes += entry.bytes();
		}

		while (m_bytes > texture_budget) {
			auto oldest = m_textures.end();
			for (auto it = m_textures.begin(); it != m_textures.end(); ++it) {
				if (it->second.state == preview_state::ready
					&& (oldest == m_textures.end() || it->second.last_frame < oldest->second.last_frame))
					oldest = it;
			}
			m_bytes -= oldest->second.bytes();
			m_textures.erase(oldest);
		}
	}

public:
	~preview_cache() {
		{
			std::lock_guard lock(m_mutex);
			m_stop = true;
		}
		m_wake.notify_one();
		if (m_worker.joinable())
			m_worker.join();
	}

	const preview_texture& get(ID3D11Device* device, int frame, unsigned int definition, unsigned int kit) {
		static const preview_texture pending;
		static const preview_texture missing{ {}, 0, 0, 0, preview_state::missing };
		if (!device)
			return missing;
		begin_frame(device, frame);
		const key_type key = (key_type(definition) << 32) | kit;
		if (const auto found = m_textures.find(key); found != m_textures.end()) {
			found->second.last_frame = frame;
			return found->second;
		}

		std::lock_guard lock(m_mutex);
		if (m_outstanding >= max_pending)
			return pending;
		if (!m_worker.joinable()) {
			try {
				m_worker = std::thread(&preview_cache::decode, this);
			} catch (const std::system_error&) {
				return missing;
			}
		}
		m_jobs.push_back(key);
		++m_outstanding;
		m_wake.notify_one();
		return m_textures.emplace(key, preview_texture{ {}, 0, 0, frame }).first->second;
	}
};
}
