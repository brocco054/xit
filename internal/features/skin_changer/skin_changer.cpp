#include "skin_changer.hpp"
#include "../shared/econ_item_attribute_manager.hpp"
#include "../shared/item_schema.hpp"
#include "../../valve/interfaces/interfaces.hpp"
#include "../../valve/interfaces/vtables/i_econ_item_system.hpp"
#include "../../menu/acervo_requests.hpp"

// define VC_ITEM_DEBUG (compiler flag) to trace knife identity resolution
// (index -> stable def -> paint kit -> model). compiled out by default.
#ifdef VC_ITEM_DEBUG
#include "../../sdk/console/console.hpp"
#include "../../sdk/includes/xor.hpp"
#endif

namespace {
bool refresh_weapon_material(c_econ_entity* weapon) {
	using clear_materials_fn = void(__fastcall*)(void*, bool);
	using rebuild_material_fn = void(__fastcall*)(c_econ_entity*, bool);

	// Resolve the two calls from the engine's per-weapon regeneration sequence.
	// The LEA in this sequence identifies the weapon's embedded render state.
	static const auto sequence = g_opcodes->scan(g_modules->m_modules.client_dll.get_name(),
		"48 8D 8B 10 06 00 00 B2 01 E8 ? ? ? ? 33 D2 48 8B CB E8 ? ? ? ? 48 8D 4C 24 30");
	if (!sequence)
		return false;

	static const auto clear_materials = reinterpret_cast<clear_materials_fn>(
		g_opcodes->get_absolute_address(sequence + 9, 1));
	static const auto rebuild_material = reinterpret_cast<rebuild_material_fn>(
		g_opcodes->get_absolute_address(sequence + 19, 1));
	clear_materials(reinterpret_cast<std::byte*>(weapon) + 0x610, true);
	rebuild_material(weapon, false);
	return true;
}

bool finish_changed(c_econ_entity* weapon, int paint_kit_id, float wear, int seed) {
	return weapon->m_paint_kit() != paint_kit_id
		|| weapon->m_wear() != wear
		|| weapon->m_seed() != seed;
}
}

c_base_entity* c_skin_changer::get_hud_weapon(c_base_entity* weapon, c_cs_player_pawn* local_pawn) {
	auto arms_handle = local_pawn->m_hud_model_arms();
	if (!arms_handle.is_valid())
		return nullptr;

	auto* hud_arms = reinterpret_cast<c_base_entity*>(
		g_interfaces->m_entity_system->get_base_entity(arms_handle.get_entry_index())
	);
	if (!valid_ptr(hud_arms))
		return nullptr;

	auto* arms_node = hud_arms->m_scene_node();
	if (!valid_ptr(arms_node))
		return nullptr;

	for (auto* vm = arms_node->m_child(); valid_ptr(vm); vm = vm->m_next_sibling()) {
		auto* vm_owner = vm->m_owner();
		if (!valid_ptr(vm_owner))
			continue;

		auto* vm_entity = reinterpret_cast<c_base_entity*>(vm_owner);
		auto owner_handle = vm_entity->m_owner_entity();
		if (!owner_handle.is_valid())
			continue;

		if (g_interfaces->m_entity_system->get_base_entity(owner_handle.get_entry_index()) == weapon)
			return vm_entity;
	}
	return nullptr;
}

void c_skin_changer::apply_skin(c_econ_entity* weapon, c_econ_item_view* item, c_base_entity* hud_weapon,
	int paint_kit_id, float wear, int seed) {
	econ_item_attribute_manager::remove(item);
	if (paint_kit_id > 0)
		econ_item_attribute_manager::create(item, paint_kit_id, wear, seed);

	weapon->m_paint_kit() = paint_kit_id;
	weapon->m_wear() = wear;
	weapon->m_seed() = seed;


	const auto definition = item->m_definition_index();
	uint64_t mesh_mask = 1;
	if (paint_kit_id > 0) {
		auto* render_kit = item->construct_paint_kit();
		if (!render_kit || render_kit->m_id != paint_kit_id) {
			acervo::report_finish(definition, paint_kit_id, acervo::request_status::material_rejected);
			return;
		}
		if (render_kit->uses_old_model())
			mesh_mask = 2;
	}

	if (auto* scene_node = weapon->m_scene_node())
		scene_node->set_mesh_group_mask(mesh_mask);

	if (hud_weapon)
		if (auto* hud_node = hud_weapon->m_scene_node())
			hud_node->set_mesh_group_mask(mesh_mask);

	const auto result = refresh_weapon_material(weapon)
		? acervo::request_status::material_requested : acervo::request_status::material_unavailable;
	acervo::report_finish(definition, paint_kit_id, result);
}

void c_skin_changer::initialize() {
	if (m_initialized)
		return;

	if (!g_item_schema->is_initialized())
		g_item_schema->initialize();

	m_initialized = g_item_schema->is_initialized();
}

void c_skin_changer::process_weapon(c_econ_entity* weapon, c_econ_item_view* item,
	c_base_entity* hud_weapon, bool force_update) {
	uint16_t def_index = item->m_definition_index();
	int config_index = c_config::skin_changer_t::get_config_index(def_index);
	if (config_index == 0)
		return;

	auto& skin = g_cfg->skin_changer.weapon_skins[config_index];
	if (skin.paint_kit == 0)
		return;

	int paint_kit_id = g_item_schema->get_paint_kit_id_for_item(def_index, skin.paint_kit);
	if (paint_kit_id == 0)
		return;

	if (!force_update && !finish_changed(weapon, paint_kit_id, skin.wear, skin.seed))
		return;

	apply_skin(weapon, item, hud_weapon, paint_kit_id, skin.wear, skin.seed);
}

void c_skin_changer::process_knife(c_econ_entity* weapon, c_econ_item_view* item,
	c_base_entity* hud_weapon, bool force_update) {
	if (!g_item_schema->is_initialized())
		return;

	const auto& config = g_cfg->knife_changer;
	const int knife_idx = config.m_knife;
	const uint16_t def_index      = item->m_definition_index();
	// same stable resolve as the menu: slot 0 / out-of-range -> def 0 ->
	// bail, so a stale or reordered index can never select the wrong knife.
	const uint16_t selected_knife =
		c_item_schema::def_from_index(g_item_schema->knives, knife_idx);
	if (selected_knife == 0)
		return;

	const int paint_kit_id = g_item_schema->get_paint_kit_id_for_item(selected_knife, config.m_paint_kit);
	const char* model_path = g_item_schema->knives[knife_idx].model_path;

#ifdef VC_ITEM_DEBUG
	LOG_INFO(xorstr_("[knife] idx=%d -> def=%u | pk_idx=%d -> pk_id=%d | model=%s"),
		knife_idx, selected_knife,
		config.m_paint_kit, paint_kit_id, model_path ? model_path : xorstr_("(none)"));
#endif
	const bool model_changed = def_index != selected_knife;
	if (!force_update && !model_changed
		&& !finish_changed(weapon, paint_kit_id, config.m_wear, config.m_seed))
		return;

	item->m_definition_index() = selected_knife;
	item->m_entity_quality() = QUALITY_UNUSUAL;

	if (model_path) {
		weapon->set_model(model_path);
		if (hud_weapon)
			hud_weapon->set_model(model_path);
	}

	if (model_changed)
		weapon->update_subclass(selected_knife);
	apply_skin(weapon, item, hud_weapon, paint_kit_id, config.m_wear, config.m_seed);
}

void c_skin_changer::run(int stage) {
	if (stage != frame_render_pass)
		return;

	const bool skin_enabled  = g_cfg->skin_changer.m_enabled;
	const bool knife_enabled = g_cfg->knife_changer.m_enabled;
	if (!skin_enabled && !knife_enabled) {
		should_update = false;
		m_last_active_weapon = {};
		m_last_hud_weapon = nullptr;
		return;
	}

	if (!g_ctx->m_local_pawn)
		return;

	auto* local_pawn = reinterpret_cast<c_cs_player_pawn*>(g_ctx->m_local_pawn);
	if (!valid_ptr(local_pawn) || local_pawn->m_health() <= 0)
		return;

	auto* weapon_service = local_pawn->m_weapon_services();
	if (!valid_ptr(weapon_service))
		return;

	auto& my_weapons   = weapon_service->my_weapons();
	auto* entity_system = g_interfaces->m_entity_system;
	const auto active_weapon = weapon_service->m_active_weapon();
	c_base_entity* active_hud_weapon = nullptr;

	for (unsigned int i = 0; i < my_weapons.m_size; i++) {
		const auto handle = my_weapons.m_elements[i];
		if (!handle.is_valid())
			continue;
		auto* weapon = reinterpret_cast<c_econ_entity*>(
			entity_system->get_base_entity(handle.get_entry_index())
		);
		if (!weapon)
			continue;

		auto* identity = weapon->m_entity();
		if (!identity || !identity->is_valid() || !identity->is_safe_to_modify())
			continue;

		auto* attr_mgr = weapon->m_attribute_manager();
		if (!valid_ptr(attr_mgr))
			continue;

		auto* item = attr_mgr->m_item();
		if (!valid_ptr(item))
			continue;

		const uint16_t def_index = item->m_definition_index();
		const bool is_knife = (def_index == WEAPON_KNIFE || def_index == WEAPON_KNIFE_T
		                   || (def_index >= 500 && def_index <= 526));

		if ((is_knife && !knife_enabled) || (!is_knife && !skin_enabled))
			continue;

		c_base_entity* hud_weapon = nullptr;
		bool force_update = should_update;
		if (handle == active_weapon) {
			hud_weapon = get_hud_weapon(weapon, local_pawn);
			active_hud_weapon = hud_weapon;
			force_update |= hud_weapon && (hud_weapon != m_last_hud_weapon || handle != m_last_active_weapon);
		}

		if (is_knife)
			process_knife(weapon, item, hud_weapon, force_update);
		else
			process_weapon(weapon, item, hud_weapon, force_update);
	}

	m_last_active_weapon = active_weapon;
	m_last_hud_weapon = active_hud_weapon;
	should_update = false;
}
