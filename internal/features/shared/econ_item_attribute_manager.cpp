#include "econ_item_attribute_manager.hpp"
#include "../../valve/classes/c_cs_player_pawn.hpp"
#include "../../valve/interfaces/vtables/i_mem_alloc.hpp"
#ifdef VC_ITEM_OBSERVE
#include "../../sdk/console/lab_diagnostics.hpp"
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <set>
#endif

namespace {
	struct econ_item_attribute_t {
		char     pad_0000[0x30];
		uint16_t def_index;
		char     pad_0032[2];
		float    value;
		float    init_value;
		int32_t  refundable_currency;
		bool     set_bonus;
		char     pad_0041[7];
	};

	struct ptr_game_vector_t {
		uint64_t  size;
		uintptr_t ptr;
	};

	enum : uint16_t {
		ATTR_PAINT   = 6,
		ATTR_PATTERN = 7,
		ATTR_WEAR    = 8,
	};
}

namespace econ_item_attribute_manager {

	void create(c_econ_item_view* item, int paint_kit, float wear, int seed) {
		if (!item || paint_kit <= 0)
			return;

		auto* attr_list = item->m_attribute_list();
		if (!attr_list)
			return;

		auto* vec = reinterpret_cast<ptr_game_vector_t*>(&attr_list->m_attributes());
		if (vec->size != 0 || vec->ptr != 0)
			return;

		constexpr size_t attr_count = 3;
		auto* attrs = static_cast<econ_item_attribute_t*>(GameAlloc(attr_count * sizeof(econ_item_attribute_t)));
		if (!attrs)
			return;

		memset(attrs, 0, attr_count * sizeof(econ_item_attribute_t));

		attrs[0].def_index  = ATTR_PAINT;
		attrs[0].value      = static_cast<float>(paint_kit);
		attrs[0].init_value = attrs[0].value;

		attrs[1].def_index  = ATTR_PATTERN;
		attrs[1].value      = static_cast<float>(seed >= 0 ? seed : 0);
		attrs[1].init_value = attrs[1].value;

		attrs[2].def_index  = ATTR_WEAR;
		attrs[2].value      = wear >= 0.0f ? wear : 0.01f;
		attrs[2].init_value = attrs[2].value;

		vec->size = attr_count;
		vec->ptr  = reinterpret_cast<uintptr_t>(attrs);
	}

	void remove(c_econ_item_view* item) {
		if (!item)
			return;

		auto* attr_list = item->m_attribute_list();
		if (!attr_list)
			return;

		auto* vec = reinterpret_cast<ptr_game_vector_t*>(&attr_list->m_attributes());
		if (vec->size == 0)
			return;

		void* ptr = reinterpret_cast<void*>(vec->ptr);
		vec->size = 0;
		vec->ptr  = 0;

		GameFree(ptr);
	}

#ifdef VC_ITEM_OBSERVE
	namespace {
		void record_item(const char* kind, c_econ_item_view& item) {
			const auto* bytes = reinterpret_cast<const unsigned char*>(&item.m_attribute_list()->m_attributes());
			uint32_t count{};
			uintptr_t data{};
			std::memcpy(&count, bytes, sizeof count);
			std::memcpy(&data, bytes + offsetof(ptr_game_vector_t, ptr), sizeof data);
			char detail[112];
			std::snprintf(detail, sizeof detail, "%s def=%u count=%u data_present=%u",
				kind, static_cast<unsigned>(item.m_definition_index()), count, static_cast<unsigned>(data != 0));
			lab_diagnostics::record("item_observed", detail);
		}
	}

	void observe_active_items(c_cs_player_pawn* pawn) {
		if (!pawn || pawn->m_health() <= 0)
			return;

		static std::set<c_base_handle> weapons;
		static std::set<std::pair<c_base_handle, uint16_t>> gloves;
		const auto pawn_handle = pawn->get_handle();
		auto* glove = pawn->m_econ_gloves();
		if (pawn_handle.is_valid() && glove && glove->m_attribute_list() && gloves.size() < 256 &&
			gloves.emplace(pawn_handle, glove->m_definition_index()).second)
			record_item("glove", *glove);

		auto* services = pawn->m_weapon_services();
		if (!services || weapons.size() >= 256)
			return;
		const auto handle = services->m_active_weapon();
		if (!handle.is_valid() || weapons.find(handle) != weapons.end())
			return;
		auto* weapon = pawn->get_active_weapon();
		if (!weapon || weapon->get_handle() != handle)
			return;
		auto* manager = weapon->m_attribute_manager();
		auto* item = manager ? manager->m_item() : nullptr;
		if (item && item->m_attribute_list()) {
			weapons.insert(handle);
			record_item("weapon", *item);
		}
	}
#endif
}
