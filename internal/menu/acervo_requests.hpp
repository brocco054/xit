#pragma once

#include "acervo_catalog.hpp"

namespace acervo {

enum class request_status {
	idle, queued, accepted, material_requested, material_unavailable, material_rejected,
	unavailable, update_unavailable, kit_blocked
};

void queue_preset(const catalog_entry& entry, int seed, float wear);
request_status status(const cosmetics::Preset& preset);
void process_pending();
void report_finish(unsigned int definition, unsigned int kit, request_status result);

}  // namespace acervo
