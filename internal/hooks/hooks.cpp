#include "../main.hpp"
#include "../directx/directx.hpp"
#include "../menu/menu.hpp"
#include "../menu/acervo_requests.hpp"
#include "../features/shared/item_schema.hpp"
#include "../features/shared/econ_item_attribute_manager.hpp"
#include "../features/skin_changer/skin_changer.hpp"
#include "../features/glove_changer/glove_changer.hpp"
#include "../valve/interfaces/vtables/i_csgo_input.hpp"
#include "../valve/interfaces/vtables/i_game_event.hpp"
#include "../valve/classes/c_cs_player_pawn.hpp"
#include "../sdk/includes/hash.hpp"
#include "../sdk/console/lab_diagnostics.hpp"
#include <atomic>

using namespace hooks;

namespace event_hashes {
	constexpr uint32_t round_start = fnv1a::hash_32("round_start");
	constexpr uint32_t item_purchase = fnv1a::hash_32("item_purchase");
}

inline uint32_t hash_event_name(const char* str) {
	uint32_t hash = fnv1a::val_32_const;
	while (*str) {
		hash = (hash ^ static_cast<uint8_t>(*str)) * fnv1a::prime_32_const;
		str++;
	}
	return hash;
}

bool c_hooks::initialize() {
	MH_Initialize();

	mouse_input_enabled::m_mouse_input_enabled.hook(vmt::get_v_method(g_interfaces->m_csgo_input, 23), mouse_input_enabled::hk_mouse_input_enabled);
	enable_cursor::m_enable_cursor.hook(vmt::get_v_method(g_interfaces->m_input_system, 76), enable_cursor::hk_enable_cursor);

	frame_stage_notify::installed = frame_stage_notify::m_frame_stage_notify.hook(
		g_opcodes->scan(g_modules->m_modules.client_dll.get_name(), "48 89 5C 24 ? 48 89 6C 24 ? 57 48 83 EC 40 48 8B F9 33 ED"),
		frame_stage_notify::hk_frame_stage_notify
	);

	{
		i_game_event::get_name = reinterpret_cast<i_game_event::GetNameFn>(g_opcodes->scan(g_modules->m_modules.client_dll.get_name(), "8B 41 14 0F BA E0 1E 73 05 48 8D 41 18 C3"));
	}

	fire_event_client_side::m_fire_event_client_side.hook(
		g_opcodes->scan(g_modules->m_modules.client_dll.get_name(), "40 53 41 54 41 56 48 83 EC ? 4C 8B F2"),
		fire_event_client_side::hk_fire_event_client_side
	);

	level_init::m_level_init.hook(
		g_opcodes->scan(g_modules->m_modules.client_dll.get_name(), "40 55 56 41 56 48 8D 6C 24 ? 48 81 EC ? ? ? ? 48 8B 0D"),
		level_init::hk_level_init
	);

	const bool present_available = g_directx->m_present_address &&
		present::m_present.hook(g_directx->m_present_address, present::hk_present);
	if (g_directx->m_resize_buffers_address)
		resize_buffers::m_resize_buffers.hook(g_directx->m_resize_buffers_address, resize_buffers::hk_resize_buffers);
	if (g_directx->m_create_swap_chain_address)
		create_swap_chain::m_create_swap_chain.hook(g_directx->m_create_swap_chain_address, create_swap_chain::hk_create_swap_chain);

	return present_available;
}

void c_hooks::destroy() {
	if (g_menu->m_opened && ImGui::GetCurrentContext()) {
		ImGui::GetIO().MouseDrawCursor = false;
		ShowCursor(TRUE);

		auto original = enable_cursor::m_enable_cursor.get_original<decltype(&enable_cursor::hk_enable_cursor)>();
		if (original) {
			__try {
				original(g_interfaces->m_input_system, enable_cursor::m_enable_cursor_input);
			}
			__except (EXCEPTION_EXECUTE_HANDLER) {}
		}
	}

	g_menu->m_opened = true;

	present::m_present.unhook();
	resize_buffers::m_resize_buffers.unhook();
	create_swap_chain::m_create_swap_chain.unhook();

	Sleep(200);

	mouse_input_enabled::m_mouse_input_enabled.unhook();
	enable_cursor::m_enable_cursor.unhook();
	frame_stage_notify::m_frame_stage_notify.unhook();
	fire_event_client_side::m_fire_event_client_side.unhook();
	level_init::m_level_init.unhook();

	Sleep(100);

	g_directx->uninitialize();

	MH_Uninitialize();
}

bool __fastcall hooks::mouse_input_enabled::hk_mouse_input_enabled(void* ptr) {
	auto original = m_mouse_input_enabled.get_original<decltype(&hk_mouse_input_enabled)>();
	return g_menu->m_opened ? false : original(ptr);
}

void* __fastcall hooks::enable_cursor::hk_enable_cursor(void* rcx, bool active) {
	auto original = m_enable_cursor.get_original<decltype(&hk_enable_cursor)>();

	m_enable_cursor_input = active;
	if (g_menu->m_opened)
		active = false;

	return original(rcx, active);
}

void hooks::frame_stage_notify::hk_frame_stage_notify(void* source_to_client, int stage) {
	auto original = m_frame_stage_notify.get_original<decltype(&hk_frame_stage_notify)>();

	if (g_interfaces && g_interfaces->m_entity_system) {
		g_ctx->m_local_pawn = g_interfaces->m_entity_system->get_local_pawn();
	}

	if (stage == frame_render_pass) {
#ifdef VC_ITEM_OBSERVE
		econ_item_attribute_manager::observe_active_items(reinterpret_cast<c_cs_player_pawn*>(g_ctx->m_local_pawn));
#else
		std::lock_guard lock(g_config_mutex);
		acervo::process_pending();
		g_skin_changer->run(stage);
		g_glove_changer->run(stage);
#endif
	}

	original(source_to_client, stage);
}

__int64 __fastcall hooks::level_init::hk_level_init(void* rcx, void* rdx) {
	auto original = m_level_init.get_original<decltype(&hk_level_init)>();

	{
		std::lock_guard lock(g_config_mutex);
		if (g_cfg->knife_changer.m_enabled || g_cfg->skin_changer.m_enabled) {
			g_skin_changer->should_update = true;
		}

		if (g_cfg->glove_changer.m_enabled) {
			g_glove_changer->should_update = true;
		}
	}

	return original(rcx, rdx);
}

bool __fastcall hooks::fire_event_client_side::hk_fire_event_client_side(void* p_game_event_manager, void* p_game_event) {
	auto original = m_fire_event_client_side.get_original<decltype(&hk_fire_event_client_side)>();

	if (!p_game_event || !i_game_event::get_name)
		return original(p_game_event_manager, p_game_event);

	const char* event_name = i_game_event::get_name(p_game_event);
	if (!event_name)
		return original(p_game_event_manager, p_game_event);

	const uint32_t event_hash = hash_event_name(event_name);

	if (event_hash == event_hashes::round_start || event_hash == event_hashes::item_purchase) {
		std::lock_guard lock(g_config_mutex);
		g_skin_changer->should_update |= g_cfg->knife_changer.m_enabled || g_cfg->skin_changer.m_enabled;
	}

	return original(p_game_event_manager, p_game_event);
}

HRESULT hooks::present::hk_present(IDXGISwapChain* swap_chain, unsigned int sync_interval, unsigned int flags) {
	auto original = m_present.get_original<decltype(&hk_present)>();

	g_directx->start_frame(swap_chain);

	auto* device_context = g_directx->get_device_context();
	auto* render_target = g_directx->get_render_target();
	if (device_context && render_target) {
		device_context->OMSetRenderTargets(1, &render_target, nullptr);

		g_directx->new_frame();

		g_menu->draw();
		g_directx->end_frame();
		lab_diagnostics::measure_frame(g_menu->m_opened);
		static std::atomic<bool> first_frame{true};
		if (first_frame.exchange(false, std::memory_order_relaxed))
			lab_diagnostics::record("first_frame");
	}

	return original(swap_chain, sync_interval, flags);
}

HRESULT hooks::resize_buffers::hk_resize_buffers(IDXGISwapChain* swap_chain, UINT buffer_count, UINT width, UINT height, DXGI_FORMAT new_format, UINT swap_chain_flags) {
	auto original = m_resize_buffers.get_original<decltype(&hk_resize_buffers)>();
	g_directx->prepare_resize(swap_chain);
	return original(swap_chain, buffer_count, width, height, new_format, swap_chain_flags);
}

HRESULT __stdcall hooks::create_swap_chain::hk_create_swap_chain(IDXGIFactory* factory, IUnknown* device, DXGI_SWAP_CHAIN_DESC* desc, IDXGISwapChain** swap_chain) {
	auto original = m_create_swap_chain.get_original<decltype(&hk_create_swap_chain)>();

	g_directx->destroy_render_target();
	return original(factory, device, desc, swap_chain);
}
