#include "../main.hpp"
#include "menu.hpp"
#include "acervo_requests.hpp"
#include "acervo_preview.hpp"
#include "acervo_kits.hpp"
#include "../directx/directx.hpp"
#include "../features/shared/item_schema.hpp"
#include "../features/skin_changer/skin_changer.hpp"
#include "../features/glove_changer/glove_changer.hpp"
#include "../sdk/config_system/config_system.hpp"
#include "../valve/classes/c_cs_player_pawn.hpp"
#include "../valve/interfaces/interfaces.hpp"
#include "../utils/utils.hpp"
#include <algorithm>
#include <cmath>
#include <mutex>
#include <optional>
#include <random>
#include <sstream>
#include <vector>
#include <string>

namespace {
	const ImVec4 vc_accent(0.77f, 0.64f, 1.0f, 1.0f);
	const ImVec4 vc_green(0.298f, 0.686f, 0.314f, 1.00f);  // #4caf50 enabled checkbox / "Dev"
	const ImVec4 vc_white(0.902f, 0.902f, 0.922f, 1.00f);
	const ImVec4 vc_muted(0.68f, 0.65f, 0.75f, 1.0f);
	const ImVec4 vc_header(0.82f, 0.77f, 0.90f, 1.0f);
	const ImVec4 vc_divider(0.22f, 0.17f, 0.30f, 1.0f);

	const ImVec4 vc_bg(0.035f, 0.029f, 0.051f, 1.0f);
	const ImVec4 vc_bar(0.102f, 0.071f, 0.157f, 1.0f);
	const ImVec4 vc_panel_l(0.068f, 0.052f, 0.092f, 1.0f);
	const ImVec4 vc_panel_m = vc_panel_l;
	const ImVec4 vc_panel_r = vc_panel_l;

	void section_header(const char* label) {
		ImGui::PushStyleColor(ImGuiCol_Text, vc_header);
		ImGui::TextUnformatted(label);
		ImGui::PopStyleColor();
		ImGui::Separator();
		ImGui::Spacing();
	}

	bool begin_panel(const char* id, float width, const char* title, const ImVec4& bg) {
		ImGui::PushStyleColor(ImGuiCol_ChildBg, bg);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 10.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
		// explicit size: caller width (>0) or remaining, full parent height
		float w = width > 0.0f ? width : ImGui::GetContentRegionAvail().x;
		float h = ImGui::GetContentRegionAvail().y;
		bool open = ImGui::BeginChild(id, ImVec2(w, h), true);
		section_header(title);
		return open;
	}

	void end_panel() {
		ImGui::EndChild();
		ImGui::PopStyleVar(2);
		ImGui::PopStyleColor();
	}

	// selectable with a manually drawn rounded highlight (ImGui::Selectable
	// can't round its own bg). selected text = accent, others = white.
	bool vc_selectable(const char* label, bool selected) {
		const ImGuiStyle& st = ImGui::GetStyle();
		ImVec2 p = ImGui::GetCursorScreenPos();
		float w = ImGui::GetContentRegionAvail().x;
		float h = ImGui::GetTextLineHeight() + st.FramePadding.y * 2.0f;
		ImVec2 mn = p, mx = ImVec2(p.x + w, p.y + h);

		bool hovered = ImGui::IsWindowHovered() && ImGui::IsMouseHoveringRect(mn, mx);
		auto* dl = ImGui::GetWindowDrawList();
		if (selected)
			dl->AddRectFilled(mn, mx, ImGui::GetColorU32(ImGuiCol_Header), 4.0f);
		else if (hovered)
			dl->AddRectFilled(mn, mx, ImGui::GetColorU32(ImGuiCol_HeaderHovered), 4.0f);

		// transparent built-in highlight (we drew our own rounded rect);
		// interaction-only selectable, label drawn manually with an 8px
		// left indent inside the highlight box
		ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
		ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
		ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
		ImGui::PushID(label);
		bool pressed = ImGui::Selectable("", selected, 0, ImVec2(w, h));
		ImGui::PopID();
		ImGui::PopStyleColor(3);

		dl->AddText(ImVec2(mn.x + 8.0f, mn.y + (h - ImGui::GetTextLineHeight()) * 0.5f),
			ImGui::GetColorU32(selected ? vc_accent : vc_white), label);
		return pressed;
	}

	// scrollable selectable list, replaces combo while keeping the same int index binding
	bool vc_list(const char* id, const char* const* items, int count, int* current) {
		bool changed = false;
		ImGui::BeginChild(id, ImVec2(0.0f, 0.0f), false);
		for (int i = 0; i < count; ++i) {
			if (vc_selectable(items[i], *current == i)) {
				*current = i;
				changed = true;
			}
		}
		ImGui::EndChild();
		return changed;
	}

	bool tab_button(const char* label, bool active) {
		ImGui::PushStyleColor(ImGuiCol_Text, active ? vc_accent : vc_muted);
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
		const bool pressed = ImGui::Button(label);
		ImGui::PopStyleColor(4);
		return pressed;
	}

	// three equal columns, accounting for the two SameLine gaps so the
	// third (fill) panel cannot be clipped
	void column_widths(float& w_left, float& w_mid) {
		const float avail = ImGui::GetContentRegionAvail().x;
		const float gap = ImGui::GetStyle().ItemSpacing.x;
		const float third = (avail - gap * 2.0f) / 3.0f;
		w_left = third;
		w_mid = third;
	}

	// dim full-width caption above an input
	void field_label(const char* text) {
		ImGui::PushStyleColor(ImGuiCol_Text, vc_muted);
		ImGui::TextUnformatted(text);
		ImGui::PopStyleColor();
	}

}

void c_menu::rebuild_fonts(float scale) {
	auto& io = ImGui::GetIO();

	io.Fonts->Clear();

	ImFontConfig config;
	config.OversampleH = 2;
	config.OversampleV = 1;

	// modern bold UI font; fall back gracefully if a face is missing
	auto load = [&](float size) -> ImFont* {
		ImFont* f = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\seguisb.ttf", size, &config);
		if (!f)
			f = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", size, &config);
		if (!f) {
			ImFontConfig def = config;
			def.SizePixels = size;
			f = io.Fonts->AddFontDefault(&def);
		}
		return f;
	};

	m_font = load(15.0f * scale);
	io.FontDefault = m_font;

	io.Fonts->Build();

	ImGui_ImplDX11_InvalidateDeviceObjects();
	ImGui_ImplDX11_CreateDeviceObjects();

	m_dpi_scale = scale;
	m_style_initialized = false;
}

void c_menu::setup_style() {
	if (m_style_initialized)
		return;

	ImGuiStyle& style = ImGui::GetStyle();
	ImVec4* colors = style.Colors;
	float scale = m_dpi_scale;

	const ImVec4 input(0.13f, 0.095f, 0.195f, 1.0f);
	const ImVec4 input_hi(0.20f, 0.145f, 0.29f, 1.0f);
	const ImVec4 hover(0.24f, 0.16f, 0.36f, 1.0f);

	colors[ImGuiCol_WindowBg] = vc_bg;
	colors[ImGuiCol_ChildBg] = vc_panel_m;
	colors[ImGuiCol_PopupBg] = vc_bar;
	colors[ImGuiCol_Border] = vc_divider;
	colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	colors[ImGuiCol_FrameBg] = input;
	colors[ImGuiCol_FrameBgHovered] = input_hi;
	colors[ImGuiCol_FrameBgActive] = input_hi;
	colors[ImGuiCol_TitleBg] = vc_bg;
	colors[ImGuiCol_TitleBgActive] = vc_bg;
	colors[ImGuiCol_TitleBgCollapsed] = vc_bg;
	colors[ImGuiCol_ScrollbarBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	colors[ImGuiCol_ScrollbarGrab] = vc_divider;
	colors[ImGuiCol_ScrollbarGrabHovered] = input_hi;
	colors[ImGuiCol_ScrollbarGrabActive] = vc_accent;
	colors[ImGuiCol_CheckMark] = vc_green;
	colors[ImGuiCol_SliderGrab] = vc_accent;
	colors[ImGuiCol_SliderGrabActive] = vc_accent;
	colors[ImGuiCol_Button] = input;
	colors[ImGuiCol_ButtonHovered] = input_hi;
	colors[ImGuiCol_ButtonActive] = hover;
	colors[ImGuiCol_Header] = hover;
	colors[ImGuiCol_HeaderHovered] = hover;
	colors[ImGuiCol_HeaderActive] = hover;
	colors[ImGuiCol_Separator] = vc_divider;
	colors[ImGuiCol_SeparatorHovered] = vc_divider;
	colors[ImGuiCol_SeparatorActive] = vc_accent;
	colors[ImGuiCol_ResizeGrip] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	colors[ImGuiCol_ResizeGripActive] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	colors[ImGuiCol_Tab] = vc_bar;
	colors[ImGuiCol_TabHovered] = hover;
	colors[ImGuiCol_TabActive] = hover;
	colors[ImGuiCol_TabUnfocused] = vc_bg;
	colors[ImGuiCol_TabUnfocusedActive] = vc_bar;
	colors[ImGuiCol_Text] = vc_white;
	colors[ImGuiCol_TextDisabled] = vc_muted;

	style.WindowRounding = 8.0f;
	style.ChildRounding = 6.0f;
	style.FrameRounding = 4.0f;
	style.PopupRounding = 4.0f;
	style.ScrollbarRounding = 4.0f;
	style.GrabRounding = 4.0f;
	style.TabRounding = 4.0f;
	style.WindowPadding = ImVec2(12.0f * scale, 12.0f * scale);
	style.FramePadding = ImVec2(8.0f * scale, 4.0f * scale);
	style.ItemSpacing = ImVec2(8.0f * scale, 6.0f * scale);
	style.ItemInnerSpacing = ImVec2(6.0f * scale, 4.0f * scale);
	style.ScrollbarSize = 5.0f * scale;
	style.GrabMinSize = 8.0f * scale;
	style.WindowBorderSize = 0.0f;
	style.ChildBorderSize = 1.0f;
	style.FrameBorderSize = 0.0f;
	style.PopupBorderSize = 1.0f;

	m_style_initialized = true;
}

namespace acervo {
namespace {
std::mutex request_mutex;
struct requested_item {
	cosmetics::Preset preset;
	request_status status;
};
std::vector<requested_item> requested_items;
request_status current_status = request_status::idle;

const catalog_entry* find_finish(const cosmetics::Preset& preset) {
	const auto entry = std::find_if(std::begin(catalog), std::end(catalog),
		[&](const catalog_entry& candidate) { return cosmetics::is_valid(preset, candidate.finish); });
	return entry == std::end(catalog) ? nullptr : entry;
}

// Resolve every item into a staging copy before publishing any part of a kit.
bool stage_preset(const cosmetics::Preset& preset, c_config& config) {
	const auto* entry = find_finish(preset);
	const auto& schema = *g_item_schema;
	if (!entry || !schema.is_initialized())
		return false;
	const auto def = static_cast<uint16_t>(preset.weapon_definition_id);
	const auto kit = schema.paint_kit_index_for_id(def, static_cast<int>(preset.paint_kit_id));
	if (!kit)
		return false;

	switch (entry->group) {
	case category::knife: {
		const int index = c_item_schema::index_of_def(schema.knives, def);
		if (!index)
			return false;
		auto& knife = config.knife_changer;
		knife.m_knife = index;
		knife.m_paint_kit = kit;
		knife.m_seed = preset.seed;
		knife.m_wear = preset.wear;
		knife.m_enabled = true;
		return true;
	}
	case category::glove: {
		const int index = c_item_schema::index_of_def(schema.gloves, def);
		if (!index)
			return false;
		auto& gloves = config.glove_changer;
		gloves.m_glove = index;
		gloves.m_paint_kit = kit;
		gloves.m_seed = preset.seed;
		gloves.m_wear = preset.wear;
		gloves.m_enabled = true;
		return true;
	}
	case category::rifle:
	case category::pistol: {
		const int slot = c_config::skin_changer_t::get_config_index(def);
		const auto item = std::find_if(schema.weapons.begin(), schema.weapons.end(),
			[def](const item_info_t& candidate) { return candidate.definition_index == def; });
		if (!slot || item == schema.weapons.end())
			return false;
		auto& weapons = config.skin_changer;
		weapons.m_selected_weapon = static_cast<int>(item - schema.weapons.begin());
		auto& finish = weapons.weapon_skins[slot];
		finish.paint_kit = kit;
		finish.seed = preset.seed;
		finish.wear = preset.wear;
		weapons.m_enabled = true;
		return true;
	}
	}
	return false;
}

void queue_items(const std::vector<cosmetics::Preset>& items) {
	std::lock_guard<std::mutex> lock(request_mutex);
	current_status = hooks::frame_stage_notify::installed
		? request_status::queued : request_status::update_unavailable;
	requested_items.clear();
	requested_items.reserve(items.size());
	for (const auto& item : items)
		requested_items.push_back({ item, current_status });
}
}

void queue_preset(const catalog_entry& entry, int seed, float wear) {
	queue_items({ { entry.finish.weapon_definition_id, entry.finish.paint_kit_id, seed, wear } });
}

request_status status(const cosmetics::Preset& preset) {
	std::lock_guard<std::mutex> lock(request_mutex);
	const auto item = std::find_if(requested_items.begin(), requested_items.end(), [&](const auto& request) {
		const auto& selected = request.preset;
		return selected.weapon_definition_id == preset.weapon_definition_id
			&& selected.paint_kit_id == preset.paint_kit_id && selected.seed == preset.seed && selected.wear == preset.wear;
	});
	return item == requested_items.end() ? request_status::idle : item->status;
}

void process_pending() {
	std::lock_guard<std::mutex> lock(request_mutex);
	if (current_status != request_status::queued)
		return;

	c_config config = *g_cfg;
	bool valid = true;
	bool gloves = false;
	bool weapons = false;
	for (auto& item : requested_items) {
		if (!stage_preset(item.preset, config)) {
			item.status = request_status::unavailable;
			valid = false;
			continue;
		}
		const bool glove = find_finish(item.preset)->group == category::glove;
		gloves |= glove;
		weapons |= !glove;
	}
	if (!valid) {
		current_status = request_status::unavailable;
		for (auto& item : requested_items) {
			if (item.status == request_status::queued)
				item.status = request_status::kit_blocked;
		}
		return;
	}

	*g_cfg = config;
	if (gloves)
		g_glove_changer->should_update = true;
	if (weapons)
		g_skin_changer->should_update = true;

	current_status = request_status::accepted;
	for (auto& item : requested_items)
		item.status = request_status::accepted;
}

void report_finish(unsigned int definition, unsigned int kit, request_status result) {
	std::lock_guard<std::mutex> lock(request_mutex);
	for (auto& item : requested_items) {
		if (item.status == request_status::accepted && item.preset.weapon_definition_id == definition
			&& item.preset.paint_kit_id == kit)
			item.status = result;
	}
}

}

#include "acervo_inventory.inl"

static void draw_weapon_panel() {
	uint16_t selected_weapon_def = 0;
	if (g_item_schema->is_initialized() &&
		g_cfg->skin_changer.m_selected_weapon >= 0 &&
		g_cfg->skin_changer.m_selected_weapon < (int)g_item_schema->weapons.size()) {
		selected_weapon_def = g_item_schema->weapons[g_cfg->skin_changer.m_selected_weapon].definition_index;
	}

	const bool has_weapon = g_item_schema->is_initialized() && selected_weapon_def > 0;
	int config_index = has_weapon ? c_config::skin_changer_t::get_config_index(selected_weapon_def) : 0;
	auto& weapon_skin = g_cfg->skin_changer.weapon_skins[config_index];

	float w_left, w_mid;
	column_widths(w_left, w_mid);

	begin_panel("skin_settings", w_left, "SKIN CHANGER", vc_panel_l);
	{
		ImGui::Checkbox("Enabled##skin", &g_cfg->skin_changer.m_enabled);

		if (g_cfg->skin_changer.m_enabled && has_weapon) {
			ImGui::Spacing();
			ImGui::TextDisabled("Wear");
			ImGui::SetNextItemWidth(-1.0f);
			ImGui::SliderFloat("##WeaponWear", &weapon_skin.wear, 0.0f, 1.0f, "%.4f");

			ImGui::TextDisabled("Seed");
			ImGui::SetNextItemWidth(-1.0f);
			if (ImGui::InputInt("##WeaponSeed", &weapon_skin.seed, 0, 0))
				weapon_skin.seed = std::clamp(weapon_skin.seed, 0, 1000);

		} else if (g_cfg->skin_changer.m_enabled) {
			ImGui::Spacing();
			ImGui::TextDisabled("Select a weapon");
		}
	}
	end_panel();
	ImGui::SameLine();

	begin_panel("skin_weapons", w_mid, "MODELS", vc_panel_m);
	{
		if (g_item_schema->is_initialized() && !g_item_schema->weapon_names_cstr.empty()) {
			vc_list("weapon_list", g_item_schema->weapon_names_cstr.data(),
				(int)g_item_schema->weapon_names_cstr.size(), &g_cfg->skin_changer.m_selected_weapon);
		} else {
			ImGui::TextDisabled("Loading weapons...");
		}
	}
	end_panel();
	ImGui::SameLine();

	begin_panel("skin_available", 0.0f, "SKINS", vc_panel_r);
	{
		if (has_weapon) {
			auto& weapon_skins = g_item_schema->get_paint_kit_names_for_item(selected_weapon_def);
			if (!weapon_skins.empty()) {
				vc_list("weapon_skin_list", weapon_skins.data(),
					(int)weapon_skins.size(), &weapon_skin.paint_kit);
			}
		}
	}
	end_panel();
}

static void draw_config_panel(float scale) {
	using acervo::profile::status;
	using acervo::profile::result;
	static std::string feedback;
	static bool failed = false;
	const auto show_result = [](const result& outcome, const char* success) {
		failed = !outcome;
		switch (outcome.code) {
		case status::ok: feedback = success; break;
		case status::invalid_name: feedback = "Nome de perfil invalido."; break;
		case status::not_found: feedback = "O perfil nao foi encontrado."; break;
		case status::conflict: feedback = "Ja existe um perfil com esse nome."; break;
		case status::io_error: feedback = "Nao foi possivel acessar a pasta ou o arquivo."; break;
		case status::invalid_data: feedback = "O perfil contem dados invalidos."; break;
		case status::unavailable: feedback = "O perfil inclui opcoes indisponiveis nesta build."; break;
		}
		if (!outcome.field.empty())
			feedback += " " + outcome.field;
	};

	float w_left, w_mid;
	column_widths(w_left, w_mid);

	const std::string& sel = g_config_system->m_selected_config;

	begin_panel("config_list", w_left, "PERFIS", vc_panel_l);
	{
		if (ImGui::IsWindowAppearing()) {
			if (const auto refreshed = g_config_system->refresh(); !refreshed)
				show_result(refreshed, "");
		}
		const auto& configs = g_config_system->get_config_files();
		if (configs.empty())
			ImGui::TextDisabled("Nenhum perfil salvo.");
		for (const auto& cfg : configs) {
			if (vc_selectable(cfg.c_str(), sel == cfg)) {
				g_config_system->m_selected_config = cfg;
				feedback.clear();
			}
		}
	}
	end_panel();
	ImGui::SameLine();

	begin_panel("config_info", w_mid, "SELECIONADO", vc_panel_m);
	{
		if (sel.empty()) {
			ImGui::TextDisabled("Selecione um perfil.");
		} else {
			ImGui::TextDisabled("Nome");
			ImGui::PushStyleColor(ImGuiCol_Text, vc_accent);
			ImGui::TextUnformatted(sel.c_str());
			ImGui::PopStyleColor();

			ImGui::Spacing();
			ImGui::TextDisabled("Novo nome");
			static char rename_buf[64] = "";
			ImGui::SetNextItemWidth(-1.0f);
			ImGui::InputText("##ConfigRename", rename_buf, sizeof(rename_buf));
			if (ImGui::Button("Renomear", ImVec2(-1.0f, 0.0f))) {
				const auto renamed = g_config_system->rename(sel, rename_buf);
				show_result(renamed, "Perfil renomeado.");
				if (renamed) {
					g_config_system->m_selected_config = rename_buf;
					rename_buf[0] = '\0';
				}
			}
		}
	}
	end_panel();
	ImGui::SameLine();

	begin_panel("config_actions", 0.0f, "ACOES", vc_panel_r);
	{
		static char config_name[64] = "";
		ImGui::TextDisabled("Novo perfil");
		ImGui::SetNextItemWidth(-1.0f);
		ImGui::InputText("##ConfigName", config_name, sizeof(config_name));

		const ImVec2 btn(-1.0f, 0.0f);
		if (ImGui::Button("Criar", btn)) {
			const auto created = g_config_system->save(config_name, false);
			show_result(created, "Perfil criado.");
			if (created) {
				g_config_system->m_selected_config = config_name;
				config_name[0] = '\0';
			}
		}

		ImGui::Spacing();
		ImGui::BeginDisabled(sel.empty());
		if (ImGui::Button("Salvar", btn))
			show_result(g_config_system->save(sel, true), "Perfil salvo.");
		if (ImGui::Button("Carregar", btn))
			show_result(g_config_system->load(sel), "Perfil carregado.");
		if (ImGui::Button("Excluir", btn))
			ImGui::OpenPopup("Excluir perfil?");
		if (ImGui::BeginPopup("Excluir perfil?")) {
			ImGui::TextWrapped("Excluir %s?", sel.c_str());
			if (ImGui::Button("Excluir perfil")) {
				const auto removed = g_config_system->remove(sel);
				show_result(removed, "Perfil excluido.");
				if (removed)
					g_config_system->m_selected_config.clear();
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancelar"))
				ImGui::CloseCurrentPopup();
			ImGui::EndPopup();
		}
		ImGui::EndDisabled();
		if (ImGui::Button("Restaurar padrao", btn)) {
			g_config_system->reset();
			show_result({}, "Configuracao padrao selecionada.");
		}
		if (ImGui::Button("Atualizar lista", btn)) {
			const auto refreshed = g_config_system->refresh();
			show_result(refreshed, "Lista atualizada.");
			const auto& configs = g_config_system->get_config_files();
			if (refreshed && std::find(configs.begin(), configs.end(), sel) == configs.end())
				g_config_system->m_selected_config.clear();
		}
		if (!feedback.empty()) {
			ImGui::PushStyleColor(ImGuiCol_Text, failed ? ImVec4(1.0f, 0.62f, 0.62f, 1.0f) : vc_accent);
			ImGui::TextWrapped("%s", feedback.c_str());
			ImGui::PopStyleColor();
		}
	}
	end_panel();
	(void)scale;
}

void c_menu::draw() {
	if (!m_opened)
		return;

	setup_style();

	ImGuiIO& io = ImGui::GetIO();
	if (io.DisplaySize.x <= 32.0f || io.DisplaySize.y <= 32.0f)
		return;
	float scale = m_dpi_scale;
	ImVec2 window_size((std::min)(880.0f * scale, io.DisplaySize.x - 16.0f),
		(std::min)(580.0f * scale, io.DisplaySize.y - 16.0f));

	ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f),
		ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(window_size, ImGuiCond_Always);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	ImGui::Begin("Hy3x by brocco###Acervo", nullptr,
		ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar);
	ImGui::PopStyleVar();

	if (m_font)
		ImGui::PushFont(m_font);

	static constexpr const char* tabs[]{ "Colecao", "Perfis", "Facas", "Luvas", "Armas", "Kits" };
	const float pad = 12.0f * scale;
	const float bar_h = 30.0f * scale;
	const float foot_h = 30.0f * scale;
	const float margin = 10.0f * scale;

	// paint the bar backgrounds manually with matching rounded corners so
	// they don't overdraw the rounded window corners (square-top bug)
	{
		const float r = 8.0f;
		ImVec2 wp = ImGui::GetWindowPos();
		ImVec2 ws = ImGui::GetWindowSize();
		auto* wdl = ImGui::GetWindowDrawList();
		const ImU32 bar_col = ImGui::GetColorU32(vc_bar);
		wdl->AddRectFilled(wp, ImVec2(wp.x + ws.x, wp.y + bar_h),
			bar_col, r, ImDrawFlags_RoundCornersTop);
		wdl->AddRectFilled(ImVec2(wp.x, wp.y + ws.y - foot_h),
			ImVec2(wp.x + ws.x, wp.y + ws.y),
			bar_col, r, ImDrawFlags_RoundCornersBottom);
	}

	// ---- top bar ----
	ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
	ImGui::BeginChild("vc_topbar", ImVec2(0.0f, bar_h), false,
		ImGuiWindowFlags_NoScrollbar);
	{
		// same font as the tab labels (the globally pushed m_font)
		ImGui::SetCursorPos(ImVec2(pad, (bar_h - ImGui::GetTextLineHeight()) * 0.5f));
		ImGui::PushStyleColor(ImGuiCol_Text, vc_white);
		ImGui::TextUnformatted("Hy3x by brocco");
		ImGui::PopStyleColor();

		const ImGuiStyle& st = ImGui::GetStyle();
		float tabs_w = 0.0f;
		for (const char* t : tabs)
			tabs_w += ImGui::CalcTextSize(t).x + st.FramePadding.x * 2.0f + 14.0f * scale;

		ImGui::SameLine();
		ImGui::SetCursorPosX(ImGui::GetWindowWidth() - tabs_w - pad);
		ImGui::SetCursorPosY((bar_h - ImGui::GetFrameHeight()) * 0.5f);
		for (int i = 0; i < IM_ARRAYSIZE(tabs); ++i) {
			if (i > 0) {
				ImGui::SameLine(0.0f, 14.0f * scale);
				ImGui::SetCursorPosY((bar_h - ImGui::GetFrameHeight()) * 0.5f);
			}
			if (tab_button(tabs[i], m_selected_tab == i))
				m_selected_tab = i;
		}
	}
	ImGui::EndChild();
	ImGui::PopStyleColor();

	ImGui::GetWindowDrawList()->AddLine(
		ImGui::GetCursorScreenPos(),
		ImVec2(ImGui::GetCursorScreenPos().x + ImGui::GetWindowWidth(), ImGui::GetCursorScreenPos().y),
		ImGui::GetColorU32(vc_divider), 1.0f);

	// ---- body: inset by a uniform margin on all four sides; bars stay
	// full window width, panels do not touch the window border ----
	ImVec2 wp = ImGui::GetWindowPos();
	ImVec2 ws = ImGui::GetWindowSize();

	float body_top = bar_h + margin;
	float body_h = ws.y - bar_h - foot_h - margin * 2.0f;
	ImGui::SetCursorPos(ImVec2(margin, body_top));
	ImGui::PushStyleColor(ImGuiCol_ChildBg, vc_bg);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	ImGui::BeginChild("vc_body", ImVec2(ws.x - margin * 2.0f, body_h), false);
	{
		if (m_selected_tab == 0)
			draw_acervo_panel();
		else if (m_selected_tab == 1) {
			std::lock_guard config_lock(g_config_mutex);
			draw_config_panel(scale);
		}
		else if (m_selected_tab == 2)
			draw_acervo_panel(acervo::category::knife);
		else if (m_selected_tab == 3)
			draw_acervo_panel(acervo::category::glove);
		else if (m_selected_tab == 4) {
			std::lock_guard config_lock(g_config_mutex);
			draw_weapon_panel();
		}
		else if (m_selected_tab == 5)
			draw_kits_panel();
	}
	ImGui::EndChild();
	ImGui::PopStyleVar();
	ImGui::PopStyleColor();

	// ---- bottom bar: fixed full-width strip pinned to window bottom,
	// same width/height/colour as the top bar ----
	float fy = ws.y - foot_h;
	ImGui::GetWindowDrawList()->AddLine(
		ImVec2(wp.x, wp.y + fy), ImVec2(wp.x + ws.x, wp.y + fy),
		ImGui::GetColorU32(vc_divider), 1.0f);

	ImGui::SetCursorPos(ImVec2(0.0f, fy));
	ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
	ImGui::BeginChild("vc_footer", ImVec2(ws.x, foot_h), false,
		ImGuiWindowFlags_NoScrollbar);
	{
		static const std::string steam_name = get_steam_username();
		if (!steam_name.empty()) {
			float ty = (foot_h - ImGui::GetTextLineHeight()) * 0.5f;
			float rw = ImGui::CalcTextSize(steam_name.c_str()).x;
			ImGui::SetCursorPos(ImVec2(ws.x - rw - margin, ty));
			ImGui::PushStyleColor(ImGuiCol_Text, vc_white);
			ImGui::TextUnformatted(steam_name.c_str());
			ImGui::PopStyleColor();
		}
	}
	ImGui::EndChild();
	ImGui::PopStyleColor();

	if (m_font)
		ImGui::PopFont();

	ImGui::End();
}
