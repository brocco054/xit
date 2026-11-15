// Included by menu.cpp so the collection can share its theme and request queue.
namespace {
using namespace acervo;

// The worker is destroyed before explicit DLL unload, outside the loader lock.
preview_cache* collection_images = nullptr;

preview_state draw_collection_image(const catalog_entry& entry, ImVec2 lo, ImVec2 hi) {
	if (!collection_images)
		collection_images = new preview_cache;
	const auto& image = collection_images->get(g_directx->get_device(), ImGui::GetFrameCount(),
		entry.finish.weapon_definition_id, entry.finish.paint_kit_id);
	if (!image.view)
		return image.state;
	const float scale = (std::min)((hi.x - lo.x) / image.width, (hi.y - lo.y) / image.height);
	const ImVec2 size(image.width * scale, image.height * scale);
	const ImVec2 start((lo.x + hi.x - size.x) * 0.5f, (lo.y + hi.y - size.y) * 0.5f);
	ImGui::GetWindowDrawList()->AddImage(reinterpret_cast<ImTextureID>(image.view.Get()),
		start, ImVec2(start.x + size.x, start.y + size.y));
	return image.state;
}

bool draw_card(const catalog_entry& entry, const char* label, ImVec2 size, bool rgb, bool focus) {
	ImGui::PushID(&entry);
	if (focus)
		ImGui::SetKeyboardFocusHere();
	const bool pressed = ImGui::InvisibleButton("##Item", size);
	const bool hovered = ImGui::IsItemHovered();
	const bool focused = ImGui::IsItemFocused();
	if (ImGui::IsItemVisible()) {
		const auto lo = ImGui::GetItemRectMin();
		const auto hi = ImGui::GetItemRectMax();
		auto* draw = ImGui::GetWindowDrawList();
		draw->AddRectFilled(lo, hi, ImGui::GetColorU32(hovered ? vc_bar : vc_panel_l), 7.0f);
		draw->AddRect(lo, hi, ImGui::GetColorU32(focused || hovered ? vc_accent : vc_divider),
			7.0f, 0, focused ? 2.0f : 1.0f);
		const float caption_height = ImGui::GetTextLineHeightWithSpacing() * 2.0f;
		const auto state = draw_collection_image(entry, ImVec2(lo.x + 7, lo.y + 4), ImVec2(hi.x - 7, hi.y - caption_height));
		if (state == preview_state::missing)
			draw->AddText(ImVec2(lo.x + 8, lo.y + 8), ImGui::GetColorU32(vc_muted), "Sem imagem");
		const ImVec2 caption(lo.x + 8, hi.y - caption_height);
		draw->PushClipRect(caption, ImVec2(hi.x - 8, hi.y - 3), true);
		draw->AddText(ImGui::GetFont(), ImGui::GetFontSize(), caption, ImGui::GetColorU32(vc_white),
			label, nullptr, size.x - 16.0f);
		draw->PopClipRect();
		if (rgb && hovered) {
			float red, green, blue;
			ImGui::ColorConvertHSVtoRGB(std::fmod(static_cast<float>(ImGui::GetTime()) * 0.16f, 1.0f),
				0.63f, 1.0f, red, green, blue);
			draw->AddRectFilled(ImVec2(lo.x + 7, hi.y - 4), ImVec2(hi.x - 7, hi.y - 2),
				ImGui::GetColorU32(ImVec4(red, green, blue, 1.0f)));
		}
	}
	if (hovered)
		ImGui::SetTooltip("%s", label);
	ImGui::PopID();
	return pressed;
}

const char* request_text(request_status value) {
	switch (value) {
	case request_status::queued: return "Aguardando o jogo.";
	case request_status::accepted: return "Validado. Aguardando o item em jogo.";
	case request_status::material_requested: return "Repintura solicitada. Inspecione o item em jogo.";
	case request_status::material_unavailable: return "Repintura indisponivel nesta build.";
	case request_status::material_rejected: return "O jogo nao resolveu a pintura do item.";
	case request_status::unavailable: return "Acabamento indisponivel nesta build.";
	case request_status::update_unavailable: return "Atualizacao de itens indisponivel nesta build.";
	case request_status::kit_blocked: return "Validado. Outro item bloqueou a aplicacao do kit.";
	case request_status::idle: return nullptr;
	}
	return nullptr;
}

struct kit_editor {
	kit_store store{ L"C:/vc/configs/kits.json" };
	std::vector<saved_kit> saved;
	std::vector<cosmetics::Preset> draft;
	std::string editing;
	std::string requested_kit;
	std::string message;
	std::vector<std::string> incompatible;
	std::optional<saved_kit> pending_edit;
	bool pending_duplicate = false;
	bool dirty = false;
	char name[80]{};
	bool loaded = false;

	void refresh() {
		if (store.read(saved, &incompatible) != kit_io::ok)
			message = "Falha ao ler os kits salvos.";
	}

	void add(const catalog_entry& entry, int seed, float wear) {
		const auto& finish = entry.finish;
		const cosmetics::Preset preset{ finish.weapon_definition_id, finish.paint_kit_id, seed, wear };
		if (!cosmetics::is_valid(preset, finish)) {
			message = "Confira a seed e o desgaste.";
			return;
		}
		dirty = true;
		const auto slot = std::find_if(draft.begin(), draft.end(), [&](const auto& existing) {
			if (entry.group == category::knife || entry.group == category::glove)
				return find_finish(existing)->group == entry.group;
			return existing.weapon_definition_id == preset.weapon_definition_id;
		});
		if (slot == draft.end())
			draft.push_back(preset);
		else {
			*slot = preset;
			message = "Item do mesmo slot substituido no rascunho.";
			return;
		}
		message = "Item adicionado ao kit.";
	}

	void start(const saved_kit& kit, bool duplicate) {
		draft = kit.items;
		editing = duplicate ? std::string{} : kit.name;
		// Keep UTF-8 valid for long names by leaving the new name blank.
		name[0] = '\0';
		if (!duplicate && kit.name.size() < sizeof(name))
			strcpy_s(name, kit.name.c_str());
		message = duplicate ? "Copia pronta. Nomeie e salve o novo kit." : "Edite os itens e salve as alteracoes.";
		dirty = duplicate;
	}

	void clear_draft() {
		draft.clear();
		editing.clear();
		name[0] = '\0';
		dirty = false;
	}

	void save() {
		switch (store.save({ name, draft }, editing)) {
		case kit_io::ok:
			message = "Kit salvo.";
			clear_draft();
			refresh();
			break;
		case kit_io::conflict: message = "Ja existe um kit com esse nome."; break;
		case kit_io::invalid: message = "Confira o nome e os itens do kit."; break;
		case kit_io::io_error: message = "Falha ao gravar o kit."; break;
		}
	}

	void copy_backup() {
		std::string backup;
		if (store.backup(backup) == kit_io::ok) {
			ImGui::SetClipboardText(backup.c_str());
			message = "Backup JSON copiado.";
		} else {
			message = "Falha ao copiar o backup.";
		}
	}

	void import_backup() {
		const auto* backup = ImGui::GetClipboardText();
		size_t added = 0;
		if (backup && store.import_backup(backup, added) == kit_io::ok) {
			message = std::to_string(added) + " kit(s) importados. Nomes existentes foram preservados.";
			refresh();
		} else {
			message = "Falha ao importar. Copie um backup JSON de kits.";
		}
	}
};

kit_editor& collection_kits() {
	static kit_editor editor;
	return editor;
}

void draw_kits_panel() {
	auto& editor = collection_kits();
	if (!editor.loaded) {
		editor.loaded = true;
		editor.refresh();
	}
	const float width = ImGui::GetContentRegionAvail().x * 0.5f;
	begin_panel("kit_draft", width, "Montar kit", vc_panel_l);
	if (!editor.editing.empty())
		ImGui::TextWrapped("Editando %s", editor.editing.c_str());
	if (editor.dirty)
		ImGui::TextDisabled("Alteracoes sem salvar");
	if (editor.draft.empty())
		ImGui::TextWrapped("Escolha um acabamento na Colecao e clique em Adicionar ao kit.");
	for (size_t i = 0; i < editor.draft.size();) {
		auto& preset = editor.draft[i];
		const auto* entry = find_finish(preset);
		ImGui::PushID(static_cast<int>(i));
		ImGui::TextWrapped("%s / %s", entry->weapon, entry->name);
		ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.58f);
		editor.dirty |= ImGui::InputInt("Seed", &preset.seed);
		preset.seed = std::clamp(preset.seed, entry->finish.min_seed, entry->finish.max_seed);
		ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.58f);
		editor.dirty |= ImGui::SliderFloat("Desgaste", &preset.wear, entry->finish.min_wear, entry->finish.max_wear, "%.4f",
			ImGuiSliderFlags_AlwaysClamp);
		const bool remove = ImGui::SmallButton("Remover");
		ImGui::PopID();
		if (remove) {
			editor.draft.erase(editor.draft.begin() + i);
			editor.dirty = true;
		} else {
			++i;
		}
		ImGui::Separator();
	}
	ImGui::SetNextItemWidth(-1.0f);
	editor.dirty |= ImGui::InputTextWithHint("##KitName", "Nome do kit", editor.name, sizeof(editor.name));
	ImGui::BeginDisabled(editor.draft.empty() || !editor.name[0]);
	if (ImGui::Button(editor.editing.empty() ? "Salvar kit" : "Salvar alteracoes", ImVec2(-1, 0)))
		editor.save();
	ImGui::EndDisabled();
	if ((!editor.draft.empty() || editor.name[0] || !editor.editing.empty())
		&& ImGui::Button("Limpar rascunho", ImVec2(-1, 0)))
		editor.clear_draft();
	if (!editor.message.empty())
		ImGui::TextWrapped("%s", editor.message.c_str());
	end_panel();
	ImGui::SameLine();

	begin_panel("kits_saved", 0.0f, "Kits salvos", vc_panel_r);
	if (ImGui::Button("Copiar backup JSON"))
		editor.copy_backup();
	ImGui::SameLine();
	if (ImGui::Button("Importar JSON copiado"))
		editor.import_backup();
	if (!editor.incompatible.empty() &&
		ImGui::TreeNode("incompatible_kits", "Kits incompativeis (%zu)", editor.incompatible.size())) {
		for (const auto& name : editor.incompatible)
			ImGui::BulletText("%s", name.c_str());
		ImGui::TextWrapped("Os registros foram preservados no backup.");
		ImGui::TreePop();
	}
	if (editor.saved.empty())
		ImGui::TextDisabled("Nenhum kit salvo.");
	std::string remove;
	bool replace_requested = false;
	for (const auto& kit : editor.saved) {
		ImGui::PushID(kit.name.c_str());
		if (ImGui::TreeNodeEx("##Kit", ImGuiTreeNodeFlags_SpanAvailWidth, "%s (%zu)", kit.name.c_str(), kit.items.size())) {
			for (const auto& item : kit.items) {
				const auto* entry = find_finish(item);
				ImGui::TextWrapped("%s / %s", entry->weapon, entry->name);
				if (editor.requested_kit == kit.name) {
					if (const auto* label = request_text(status(item)))
						ImGui::TextWrapped("%s", label);
				}
			}
#ifdef VC_ITEM_OBSERVE
			ImGui::TextDisabled("Modo de observacao. Aplicacao de kits desativada.");
#else
			if (ImGui::Button("Aplicar kit")) {
				editor.requested_kit = kit.name;
				queue_items(kit.items);
			}
			ImGui::SameLine();
#endif
			const bool edit = ImGui::Button("Editar");
			ImGui::SameLine();
			const bool duplicate = ImGui::Button("Duplicar");
			if (edit || duplicate) {
				if (editor.dirty) {
					editor.pending_edit = kit;
					editor.pending_duplicate = duplicate;
					replace_requested = true;
				} else {
					editor.start(kit, duplicate);
				}
			}
			ImGui::SameLine();
			if (ImGui::Button("Excluir"))
				ImGui::OpenPopup("Excluir kit?");
			if (ImGui::BeginPopup("Excluir kit?")) {
				ImGui::TextWrapped("Excluir %s?", kit.name.c_str());
				if (ImGui::Button("Excluir kit")) {
					remove = kit.name;
					ImGui::CloseCurrentPopup();
				}
				ImGui::SameLine();
				if (ImGui::Button("Cancelar"))
					ImGui::CloseCurrentPopup();
				ImGui::EndPopup();
			}
			ImGui::TreePop();
		}
		ImGui::PopID();
	}
	if (replace_requested)
		ImGui::OpenPopup("Alteracoes sem salvar");
	if (ImGui::BeginPopupModal("Alteracoes sem salvar", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::TextUnformatted("Descartar as alteracoes do rascunho?");
		if (ImGui::Button("Descartar")) {
			editor.start(*editor.pending_edit, editor.pending_duplicate);
			editor.pending_edit.reset();
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("Continuar editando") || ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
			editor.pending_edit.reset();
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
	if (!remove.empty()) {
		editor.message = editor.store.remove(remove) == kit_io::ok ? "Kit excluido." : "Falha ao excluir o kit.";
		editor.refresh();
	}
	end_panel();
}

std::wstring search_key(const std::string& text) {
	if (text.empty())
		return {};
	const auto length = static_cast<int>(text.size());
	const int count = MultiByteToWideChar(CP_UTF8, 0, text.data(), length, nullptr, 0);
	std::wstring wide(count, L'\0');
	if (!count || !MultiByteToWideChar(CP_UTF8, 0, text.data(), length, wide.data(), count))
		return {};
	CharLowerBuffW(wide.data(), static_cast<DWORD>(wide.size()));
	const int decomposed_count = FoldStringW(MAP_COMPOSITE, wide.data(), count, nullptr, 0);
	if (!decomposed_count)
		return wide;
	std::wstring decomposed(decomposed_count, L'\0');
	if (!FoldStringW(MAP_COMPOSITE, wide.data(), count, decomposed.data(), decomposed_count))
		return wide;
	std::vector<WORD> types(decomposed.size());
	if (!GetStringTypeW(CT_CTYPE3, decomposed.data(), decomposed_count, types.data()))
		return wide;
	std::wstring folded;
	for (size_t i = 0; i < decomposed.size(); ++i) {
		if (!(types[i] & C3_NONSPACING))
			folded += decomposed[i];
	}
	return folded;
}

struct collection_card {
	const catalog_entry* entry;
	std::string label;
	bool opens_finish;
};

std::vector<collection_card> find_cards(const std::string& search, std::optional<category> shortcut,
	category group, unsigned int model) {
	static const auto index = [] {
		std::vector<collection_card> result;
		for (const auto& entry : catalog)
			result.push_back({ &entry, std::string(entry.weapon) + " / " + entry.name, true });
		return result;
	}();
	static const auto keys = [] {
		std::vector<std::wstring> result;
		for (const auto& card : index) {
			const auto& finish = card.entry->finish;
			const auto def = std::to_string(finish.weapon_definition_id);
			const auto kit = std::to_string(finish.paint_kit_id);
			result.push_back(search_key(card.label + " " + def + " " + kit + " " + def + "/" + kit));
		}
		return result;
	}();
	std::wistringstream words(search_key(search));
	std::vector<std::wstring> terms;
	for (std::wstring term; words >> term;)
		terms.push_back(std::move(term));

	std::vector<collection_card> result;
	unsigned int previous = 0;
	for (size_t i = 0; i < index.size(); ++i) {
		const auto& card = index[i];
		if (!terms.empty()) {
			if ((!shortcut || card.entry->group == *shortcut)
				&& std::all_of(terms.begin(), terms.end(), [&](const auto& term) { return keys[i].find(term) != std::wstring::npos; }))
				result.push_back(card);
			continue;
		}
		if (card.entry->group != group)
			continue;
		const auto definition = card.entry->finish.weapon_definition_id;
		if (model ? definition != model : definition == previous)
			continue;
		previous = definition;
		result.push_back({ card.entry, model ? card.entry->name : card.entry->weapon, model != 0 });
	}
	return result;
}

const char* wear_condition(float wear) {
	if (wear < 0.07f) return "Factory New";
	if (wear < 0.15f) return "Minimal Wear";
	if (wear < 0.38f) return "Field-Tested";
	if (wear < 0.45f) return "Well-Worn";
	return "Battle-Scarred";
}

void draw_reference(const catalog_entry& entry, float height) {
	const auto lo = ImGui::GetCursorScreenPos();
	const auto width = ImGui::GetContentRegionAvail().x;
	const auto state = draw_collection_image(entry, lo, ImVec2(lo.x + width, lo.y + height));
	ImGui::Dummy(ImVec2(width, height));
	if (state == preview_state::missing)
		ImGui::TextWrapped("Imagem de referencia indisponivel.");
	else if (state == preview_state::pending)
		ImGui::TextDisabled("Carregando imagem...");
}

constexpr auto preferences_path = L"C:\\vc\\configs\\painel.ini";

void draw_acervo_panel(std::optional<category> shortcut = std::nullopt) {
	static constexpr const char* groups[]{ "Rifles", "Pistolas", "Facas", "Luvas" };
	static category group = category::rifle;
	static std::optional<category> last_shortcut;
	static unsigned int model = 0;
	static const catalog_entry* selected = nullptr;
	static const catalog_entry* submitted = nullptr;
	static int submitted_seed = 0;
	static float submitted_wear = 0.0f;
	static int seed = 1;
	static float wear = 0.03f;
	static bool motion = GetPrivateProfileIntW(L"Painel", L"AnimacaoRGB", 1, preferences_path) != 0;
	static bool preference_error = false;
	static bool reset_scroll = false;
	static bool refresh_cards = true;
	static std::mt19937 random(std::random_device{}());
	static char filter[128]{};
	static std::vector<collection_card> cards;
	static const catalog_entry* focused_card = nullptr;
	static const catalog_entry* comparison = nullptr;
	static int comparison_seed = 0;
	static float comparison_wear = 0;

	const auto change_model = [&](unsigned int next) {
		model = next;
		selected = nullptr;
		refresh_cards = true;
		reset_scroll = true;
	};
	if (shortcut != last_shortcut) {
		if (shortcut)
			group = *shortcut;
		change_model(0);
	}
	last_shortcut = shortcut;

	if (!shortcut) {
		for (int i = 0; i < IM_ARRAYSIZE(groups); ++i) {
			if (i > 0)
				ImGui::SameLine();
			const auto next = static_cast<category>(i);
			if (tab_button(groups[i], group == next) && group != next) {
				group = next;
				change_model(0);
			}
		}
		ImGui::Separator();
	}

	const auto* heading = groups[static_cast<int>(group)];
	if (model) {
		if (ImGui::Button("Voltar"))
			change_model(0);
		ImGui::SameLine();
		if (model) {
			const auto entry = std::find_if(std::begin(catalog), std::end(catalog),
				[&](const auto& candidate) { return candidate.finish.weapon_definition_id == model; });
			if (entry != std::end(catalog))
				heading = entry->weapon;
		}
	}
	ImGui::AlignTextToFramePadding();
	ImGui::TextColored(vc_accent, "%s", heading);
	ImGui::SameLine();
	if (ImGui::Checkbox("Animacao RGB", &motion)) {
		std::error_code error;
		std::filesystem::create_directories(std::filesystem::path(preferences_path).parent_path(), error);
		preference_error = error || !WritePrivateProfileStringW(L"Painel", L"AnimacaoRGB",
			motion ? L"1" : L"0", preferences_path);
	}
	ImGui::SameLine();
	if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_F, false))
		ImGui::SetKeyboardFocusHere();
	ImGui::SetNextItemWidth(-1.0f);
	if (ImGui::InputTextWithHint("##AcervoSearch", "Buscar modelo, acabamento ou ID", filter, IM_ARRAYSIZE(filter))) {
		refresh_cards = true;
		reset_scroll = true;
	}
	if (preference_error)
		ImGui::TextDisabled("Falha ao salvar a preferencia de animacao.");
	if (refresh_cards) {
		cards = find_cards(filter, shortcut, group, model);
		refresh_cards = false;
	}
	ImGui::TextDisabled("%zu resultado(s)", cards.size());

	const bool popup_open = ImGui::IsPopupOpen("Acabamento");
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImGui::GetStyle().ItemSpacing);
	ImGui::BeginChild("acervo_inventory", ImVec2(0, 0), true, ImGuiWindowFlags_AlwaysVerticalScrollbar);
	ImGui::PopStyleVar();
	if (reset_scroll) {
		ImGui::SetScrollY(0);
		reset_scroll = false;
	}
	const float font = ImGui::GetFontSize();
	const float gap = ImGui::GetStyle().ItemSpacing.x;
	const float available = ImGui::GetContentRegionAvail().x;
	const int columns = std::clamp(static_cast<int>((available + gap) / (font * 9.7f + gap)), 1, 5);
	const ImVec2 size((available - gap * (columns - 1)) / columns, font * 8.5f);
	const collection_card* activated = nullptr;
	ImGuiListClipper rows;
	rows.Begin((static_cast<int>(cards.size()) + columns - 1) / columns, size.y + ImGui::GetStyle().ItemSpacing.y);
	while (rows.Step()) {
		for (int row = rows.DisplayStart; row < rows.DisplayEnd; ++row) {
			const int start = row * columns;
			const int stop = (std::min)(start + columns, static_cast<int>(cards.size()));
			for (int i = start; i < stop; ++i) {
				if (i != start)
					ImGui::SameLine();
				const auto& card = cards[i];
				const bool focus = !popup_open && focused_card == card.entry;
				if (draw_card(*card.entry, card.label.c_str(), size, motion, focus))
					activated = &card;
				if (focus)
					focused_card = nullptr;
			}
		}
	}
	if (cards.empty())
		ImGui::TextWrapped("Nenhum resultado. Tente outro termo.");
	ImGui::EndChild();

	if (activated) {
		if (!activated->opens_finish) {
			change_model(activated->entry->finish.weapon_definition_id);
		} else {
			if (selected != activated->entry) {
				selected = activated->entry;
				seed = selected->finish.min_seed;
				wear = std::clamp(0.03f, selected->finish.min_wear, selected->finish.max_wear);
			}
			focused_card = selected;
			ImGui::OpenPopup("Acabamento");
		}
	}

	const auto display = ImGui::GetIO().DisplaySize;
	ImGui::SetNextWindowSizeConstraints(ImVec2(0, 0), ImVec2(display.x - 16, display.y - 16));
	ImGui::SetNextWindowSize(ImVec2((std::min)(font * (comparison ? 40.0f : 29.0f), display.x - 16), 0));
	bool detail_open = true;
	if (ImGui::BeginPopupModal("Acabamento", &detail_open, ImGuiWindowFlags_NoSavedSettings
		| ImGuiWindowFlags_AlwaysAutoResize)) {
		if (!selected) {
			ImGui::CloseCurrentPopup();
			ImGui::EndPopup();
			return;
		}
		ImGui::PushStyleColor(ImGuiCol_Text, vc_accent);
		ImGui::TextWrapped("%s", selected->name);
		ImGui::PopStyleColor();
		ImGui::TextDisabled("%s", selected->weapon);
		ImGui::TextWrapped("Imagens de referencia. Seed e desgaste sao visiveis no jogo.");
		const float height = font * 6.0f;
		if (comparison && ImGui::BeginTable("comparacao", 2, ImGuiTableFlags_BordersInnerV)) {
			ImGui::TableNextColumn();
			ImGui::TextWrapped("A  %s / %s", comparison->weapon, comparison->name);
			ImGui::TextWrapped("Seed %d | %.4f | %s", comparison_seed, comparison_wear, wear_condition(comparison_wear));
			draw_reference(*comparison, height);
			ImGui::TableNextColumn();
			ImGui::TextWrapped("B  %s / %s", selected->weapon, selected->name);
			ImGui::TextWrapped("Seed %d | %.4f | %s", seed, wear, wear_condition(wear));
			draw_reference(*selected, height);
			ImGui::EndTable();
		} else if (!comparison) {
			draw_reference(*selected, height);
		}
		if (ImGui::Button("Fixar como A")) {
			comparison = selected;
			comparison_seed = seed;
			comparison_wear = wear;
		}
		if (comparison) {
			ImGui::SameLine();
			if (ImGui::Button("Remover A"))
				comparison = nullptr;
		}
		const auto& finish = selected->finish;
		const auto matches_item = [&](const pattern_preset& pattern) {
			return cosmetics::is_valid(pattern.preset, finish);
		};
		const auto* pattern = std::find_if(std::begin(pattern_presets), std::end(pattern_presets), matches_item);
		if (pattern != std::end(pattern_presets)) {
			const char* label = "Personalizado";
			for (const auto& candidate : pattern_presets)
				if (matches_item(candidate) && candidate.preset.seed == seed && candidate.preset.wear == wear)
					label = candidate.label;
			field_label("Padroes de colecionador");
			ImGui::SetNextItemWidth(-1.0f);
			if (ImGui::BeginCombo("##AcervoPatterns", label)) {
				for (const auto& candidate : pattern_presets) {
					if (matches_item(candidate) && ImGui::Selectable(candidate.label, label == candidate.label)) {
						seed = candidate.preset.seed;
						wear = candidate.preset.wear;
					}
				}
				ImGui::EndCombo();
			}
		}
		field_label("Desgaste");
		ImGui::SetNextItemWidth(-1.0f);
		ImGui::SliderFloat("##AcervoWear", &wear, finish.min_wear, finish.max_wear, "%.4f", ImGuiSliderFlags_AlwaysClamp);
		ImGui::TextDisabled("%s | faixa %.4f a %.4f", wear_condition(wear), finish.min_wear, finish.max_wear);
		field_label("Seed do padrao");
		ImGui::SetNextItemWidth(-1.0f);
		ImGui::InputInt("##AcervoSeed", &seed, 1, 10);
		seed = std::clamp(seed, finish.min_seed, finish.max_seed);
		if (ImGui::Button("Sortear seed e desgaste", ImVec2(-1, 0))) {
			seed = std::uniform_int_distribution<int>(finish.min_seed, finish.max_seed)(random);
			wear = std::uniform_real_distribution<float>(finish.min_wear, finish.max_wear)(random);
		}

		ImGui::Spacing();
#ifdef VC_ITEM_OBSERVE
		ImGui::TextWrapped("Modo de observacao. Aplicacao de skins desativada.");
#else
		ImGui::PushStyleColor(ImGuiCol_Text, vc_bg);
		ImGui::PushStyleColor(ImGuiCol_Button, vc_accent);
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.88f, 0.77f, 1.0f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, vc_accent);
		if (ImGui::Button("Aplicar no jogo", ImVec2(-1, ImGui::GetFrameHeight() * 1.6f))) {
			collection_kits().requested_kit.clear();
			queue_preset(*selected, seed, wear);
			submitted = selected;
			submitted_seed = seed;
			submitted_wear = wear;
		}
		ImGui::PopStyleColor(4);
#endif
		if (ImGui::Button("Adicionar ao kit", ImVec2(-1, 0)))
			collection_kits().add(*selected, seed, wear);
		if (const auto count = collection_kits().draft.size(); count)
			ImGui::TextDisabled(count == 1 ? "%zu item em Kits." : "%zu itens em Kits.", count);
		ImGui::Spacing();
		const bool matches_submission = collection_kits().requested_kit.empty()
			&& submitted == selected && submitted_seed == seed && submitted_wear == wear;
		if (matches_submission) {
			const cosmetics::Preset preset{ finish.weapon_definition_id, finish.paint_kit_id, seed, wear };
			if (const auto* label = request_text(status(preset)))
				ImGui::TextWrapped("%s", label);
		}
		if (ImGui::Button("Voltar ao inventario"))
			ImGui::CloseCurrentPopup();
		ImGui::EndPopup();
	}
}
}  // namespace

void c_menu::release_resources() {
	delete collection_images;
	collection_images = nullptr;
}
