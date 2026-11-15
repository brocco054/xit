#include "glove_changer.hpp"
#include "../shared/econ_item_attribute_manager.hpp"
#include "../../valve/interfaces/vtables/i_econ_item_system.hpp"
#include "../../menu/acervo_requests.hpp"

// define VC_ITEM_DEBUG (compiler flag) to trace the glove resolve pipeline:
// list index -> stable definition index -> paint-kit index -> paint-kit id
// -> model path. left in, compiled out by default so it is silent in normal
// builds but available to verify identity end-to-end without code changes.
#ifdef VC_ITEM_DEBUG
#include "../../sdk/console/console.hpp"
#include "../../sdk/includes/xor.hpp"
#endif

void c_glove_changer::run(int stage) {
	if (!g_cfg->glove_changer.m_enabled || stage != frame_render_pass || !g_ctx->m_local_pawn)
		return;

	auto* local_pawn = reinterpret_cast<c_cs_player_pawn*>(g_ctx->m_local_pawn);
	if (!valid_ptr(local_pawn) || local_pawn->m_health() <= 0)
		return;

	auto* identity = local_pawn->m_entity();
	if (!identity || !identity->is_valid() || !identity->is_safe_to_modify())
		return;

	if (g_cfg->glove_changer.m_glove == 0)
		return;

	auto* glove_item = local_pawn->m_econ_gloves();
	if (!glove_item)
		return;

	if (!g_item_schema->is_initialized())
		return;

	// resolve the stable definition index from the current list position.
	// def_from_index() maps slot 0 / out-of-range -> 0 (the "(None)"
	// sentinel), so a stale or reordered index degrades to "no override"
	// instead of silently pointing at the wrong glove family. this is the
	// exact same resolve the menu uses, so UI and runtime cannot disagree.
	const uint16_t selected_glove =
		c_item_schema::def_from_index(g_item_schema->gloves, g_cfg->glove_changer.m_glove);
	if (selected_glove == 0)
		return;

	// get_paint_kit_id_for_item() only returns ids from selected_glove's own
	// compatibility set (built by file-existence probe in item_schema); an
	// index that doesn't belong resolves to 0 (Default) rather than
	// borrowing a paint kit from another glove family.
	int paint_kit_id = g_item_schema->get_paint_kit_id_for_item(selected_glove, g_cfg->glove_changer.m_paint_kit);

#ifdef VC_ITEM_DEBUG
	LOG_INFO(xorstr_("[glove] idx=%d -> def=%u | pk_idx=%d -> pk_id=%d | model=%s"),
		g_cfg->glove_changer.m_glove, selected_glove,
		g_cfg->glove_changer.m_paint_kit, paint_kit_id,
		(g_cfg->glove_changer.m_glove > 0
			&& g_cfg->glove_changer.m_glove < (int)g_item_schema->gloves.size()
			&& g_item_schema->gloves[g_cfg->glove_changer.m_glove].model_path)
			? g_item_schema->gloves[g_cfg->glove_changer.m_glove].model_path
			: xorstr_("(engine-driven glove model)"));
#endif

	const float current_spawn_time = local_pawn->m_last_spawn_time_index();
	const int   current_team       = local_pawn->m_team_num();
	const auto  current_arms       = local_pawn->m_hud_model_arms();
	if (!current_arms.is_valid())
		return;

	const bool config_changed     = (m_last_glove != selected_glove)
	                             || (m_last_paint_kit_id != paint_kit_id)
	                             || (m_last_wear != g_cfg->glove_changer.m_wear)
	                             || (m_last_seed != g_cfg->glove_changer.m_seed);
	const bool pawn_state_changed = current_team != m_last_team
	                             || current_spawn_time != m_last_spawn_time
	                             || current_arms != m_last_arms;
	// A pending rebuild is also set by this code. Reapply only when the
	// selected model or its render state has actually changed.
	if (!config_changed && !pawn_state_changed && !should_update
		&& glove_item->m_definition_index() == selected_glove && glove_item->m_initialized())
		return;

	glove_item->m_definition_index() = selected_glove;
	glove_item->m_entity_quality()   = QUALITY_UNUSUAL;

	econ_item_attribute_manager::remove(glove_item);
	if (paint_kit_id > 0)
		econ_item_attribute_manager::create(glove_item, paint_kit_id,
			g_cfg->glove_changer.m_wear, g_cfg->glove_changer.m_seed);

	glove_item->m_initialized() = true;
	const auto* material = glove_item->construct_paint_kit();
	const bool resolved = paint_kit_id == 0 || (material && material->m_id == paint_kit_id);
	if (resolved) {
		// Select the glove body group once before asking the engine to rebuild
		// the material. The render flag alone does not select the geometry.
		local_pawn->set_body_group();
		local_pawn->m_need_to_reapply_gloves() = true;
	}
	acervo::report_finish(selected_glove, paint_kit_id, resolved
		? acervo::request_status::material_requested : acervo::request_status::material_rejected);

	m_last_glove          = selected_glove;
	m_last_paint_kit_id   = paint_kit_id;
	m_last_wear           = g_cfg->glove_changer.m_wear;
	m_last_seed           = g_cfg->glove_changer.m_seed;
	m_last_spawn_time     = current_spawn_time;
	m_last_team           = current_team;
	m_last_arms           = current_arms;
	should_update = false;
}
