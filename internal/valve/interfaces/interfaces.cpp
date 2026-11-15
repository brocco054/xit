#include "../../main.hpp"

void* get_entity_by_index(int index) {
	if (!g_interfaces->m_entity_system)
		return nullptr;
	return g_interfaces->m_entity_system->get_base_entity(index);
}

#define CHECK(name, arg) \
    if (arg == nullptr) { \
        LOG_ERROR(xorstr_("[-] Failed to get: %s"), name); \
        MessageBoxA(NULL, name, "Failed to initialize", MB_OK | MB_ICONERROR); \
        return; \
    }

void c_interfaces::initialize()
{
	const char* client_dll = g_modules->m_modules.client_dll.get_name();

	using get_input_t = i_csgo_input * (__fastcall*)();
	get_input_t get_input = reinterpret_cast<get_input_t>(g_opcodes->scan_absolute(client_dll, xorstr_("E8 ? ? ? ? 48 8B 56 ? 48 8B C8 E8 ? ? ? ? 4C 89 7E"), 0x1));
	CHECK(xorstr_("Input"), get_input)
	m_csgo_input = get_input();
	CHECK(xorstr_("Input instance"), m_csgo_input)

	// A failed scan returns null; dereferencing it here crashed the game instead of reporting.
	const auto entity_system_slot = reinterpret_cast<i_entity_system**>(g_opcodes->scan_absolute(client_dll, xorstr_("48 8B 0D ? ? ? ? 48 89 7C 24 ? 8B FA C1 EB"), 0x3));
	CHECK(xorstr_("Entity System slot"), entity_system_slot)
	m_entity_system = *entity_system_slot;
	CHECK(xorstr_("Entity System"), m_entity_system);

	m_schema_system = get_interface<i_schema_system>(&g_modules->m_modules.schemasystem_dll, xorstr_("SchemaSystem_001"));
	CHECK(xorstr_("Schema System"), m_schema_system);

	m_input_system = get_interface(&g_modules->m_modules.input_system, xorstr_("InputSystemVersion001"));
	CHECK(xorstr_("Input System"), m_input_system);

	m_file_system = get_interface<i_file_system>(&g_modules->m_modules.filesystem_stdio, xorstr_("VFileSystem017"));
	CHECK(xorstr_("FileSystem"), m_file_system);
}

c_econ_item_schema* c_interfaces::get_econ_item_schema() const {
	using get_item_system_fn = c_econ_item_system* (*)();
	// CS2 build 26598758: the constructor stores CCStrike15ItemSchema at +8.
	// Source2Client002 slot 128 points to an unrelated routine on this build.
	static const auto get_item_system = reinterpret_cast<get_item_system_fn>(g_opcodes->scan(
		g_modules->m_modules.client_dll.get_name(),
		"48 83 EC 28 48 8B 05 ? ? ? ? 48 85 C0 0F 85 81 00 00 00 48 89 5C 24 30 B9 10 00 00 00 "
		"48 89 7C 24 20 E8 ? ? ? ? 33 FF 48 8B D8 48 85 C0 74 4F 48 8D 05 ? ? ? ? 48 89 7B 08 B9 78 09 00 00"));
	const auto system = get_item_system ? get_item_system() : nullptr;
	return system ? system->get_econ_item_schema() : nullptr;
}
