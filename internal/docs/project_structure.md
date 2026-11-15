# Project Structure

```
entry.cpp                         DLL entry point; attach thread; init/terminate ordering
main.hpp                          Common includes and global accessors
config.hpp                        Runtime config state structs (knife/glove/skin)

hooks/
  hooks.hpp / hooks.cpp           Hook declarations, installation, per-frame dispatch

directx/
  directx.hpp / directx.cpp       Swap-chain discovery, WndProc subclass, ImGui backend, DPI

menu/
  menu.hpp / menu.cpp             Theme, font atlas, tab + three-panel render tree

sdk/
  config_system/
    config_system.hpp / .cpp      Registration-based JSON persistence
  includes/                       Vendored third-party libraries
    imgui/                        Dear ImGui + DX11/Win32 backends + FreeType
    minhook/                      Inline hooking
    protobufs/                    Protobuf runtime + headers/libs
    json.hpp, xor.hpp, hash.hpp   JSON, compile-time string XOR, FNV hash

features/
  skin_changer/                   Knife + weapon finish application (shared system)
  glove_changer/                  Glove application (frame-window state machine)
  shared/
    item_schema.hpp / .cpp        Knife/glove/weapon + paint-kit catalog
    econ_item_attribute_manager.* Econ attribute construction/removal

valve/
  interfaces/                     Engine/client interface resolution
    vtables/                      Interface wrappers + protobuf message types
  modules/                        Loaded-module base resolution
  classes/                        Game class / pawn bindings
  schema/                         Netvar / field offset resolution

utils/
  utils.hpp / utils.cpp           Pattern scanning; Steam persona resolution

docs/                             Engineering documentation
vc.sln / vc.vcxproj           Visual Studio solution/project (output: CS.dll)
```

## Responsibilities

- **entry / main / config.hpp** - process lifecycle and the in-memory
  state that the UI binds to and the config system persists.
- **hooks** - the only place external control flow enters the module.
- **directx** - render plumbing; no gameplay logic.
- **menu** - all ImGui construction; reads/writes config fields only.
- **sdk/config_system** - the single persistence authority.
- **features** - the only place engine entities are mutated, always from
  the stage-7 dispatch.
- **valve** - typed access to engine memory and interfaces.
- **utils** - engine-agnostic helpers.

## Build Output

`vc.vcxproj` targets `DynamicLibrary`, MSVC v143, C++17, x64, and emits
`x64/Release/CS.dll`. Third-party code is vendored; there are no external
package dependencies.
