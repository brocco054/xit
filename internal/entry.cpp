#include "main.hpp"
#include "valve/interfaces/vtables/i_mem_alloc.hpp"
#include "directx/directx.hpp"
#include "features/shared/item_schema.hpp"
#include "features/skin_changer/skin_changer.hpp"
#include "features/glove_changer/glove_changer.hpp"
#include "sdk/config_system/config_system.hpp"
#include "sdk/console/lab_diagnostics.hpp"
#include <shellapi.h>

namespace {
bool insecure_session() noexcept {
    int count = 0;
    wchar_t** arguments = CommandLineToArgvW(GetCommandLineW(), &count);
    if (!arguments)
        return false;
    bool found = false;
    for (int index = 1; index < count; ++index) {
        if (_wcsicmp(arguments[index], L"-insecure") == 0) {
            found = true;
            break;
        }
    }
    LocalFree(arguments);
    return found;
}
}

void destroy(HMODULE h_module, bool requested = false) {
    g_hooks->destroy();
    logger::shutdown();
    if (requested)
        lab_diagnostics::record("session_closed");

    FreeLibraryAndExitThread(h_module, 0);
}

uintptr_t __stdcall start_address(const HMODULE h_module) {
    try {
        lab_diagnostics::record("initializing", ACERVO_VERSION);
        if (!insecure_session()) {
            lab_diagnostics::record("startup_error", "insecure_flag_required");
            FreeLibraryAndExitThread(h_module, 1);
        }
        logger::initialize();

        g_modules->m_modules.initialize();
        g_interfaces->initialize();
        // initialize() returns early on the first failed lookup; never hook on a partial set.
        if (!g_interfaces->m_csgo_input || !g_interfaces->m_entity_system || !g_interfaces->m_schema_system
            || !g_interfaces->m_input_system || !g_interfaces->m_file_system) {
            lab_diagnostics::record("startup_error", "interfaces_unavailable");
            logger::shutdown();
            FreeLibraryAndExitThread(h_module, 1);
        }

        g_item_schema->initialize();
        if (!g_item_schema->is_initialized()) {
            lab_diagnostics::record("startup_error", "catalog_unavailable");
            MessageBoxW(nullptr, L"O cat\u00e1logo de itens n\u00e3o est\u00e1 dispon\u00edvel. Abra Ancient antes de iniciar o painel.",
                L"Hy3x by brocco", MB_OK | MB_ICONWARNING);
            logger::shutdown();
            FreeLibraryAndExitThread(h_module, 1);
        }
        lab_diagnostics::record("catalog_ready");
#ifdef VC_ITEM_OBSERVE
        lab_diagnostics::record("item_observation_mode");
#endif
        g_skin_changer->initialize();

        g_config_system->setup_values();
        g_config_system->auto_load();

        g_directx->initialize();
        if (!g_hooks->initialize()) {
            lab_diagnostics::record("startup_error", "first_frame_callback_unavailable");
            destroy(h_module);
        }

        LOG_SUCCESS(xorstr_("vc initialized"));

        // 0x8000 = key currently down; the bare return also fires on a press made earlier, in any window.
        while (!(GetAsyncKeyState(VK_END) & 0x8000)) {
            Sleep(100);
        }

        lab_diagnostics::record("shutdown_requested");
        destroy(h_module, true);
    }
    catch (const std::exception& e) {
        lab_diagnostics::record("startup_error", e.what());
        MessageBoxA(NULL, e.what(), "Exception", MB_OK | MB_ICONERROR);
    }
    catch (...) {
        lab_diagnostics::record("startup_error", "unknown_exception");
        MessageBoxA(NULL, "Unknown exception occurred", "Exception", MB_OK | MB_ICONERROR);
    }

    return 0;
}

BOOL APIENTRY DllMain(HMODULE h_module, DWORD ul_reason_for_call, LPVOID lp_reserved) {
    if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
        InitMemAlloc();
        DisableThreadLibraryCalls(h_module);

        HANDLE thread = CreateThread(nullptr, 0,
                                     reinterpret_cast<LPTHREAD_START_ROUTINE>(start_address),
                                     h_module, 0, nullptr);

        if (thread != nullptr && thread != INVALID_HANDLE_VALUE) {
            CloseHandle(thread);
            return TRUE;
        }

        return FALSE;
    }

    return TRUE;
}
