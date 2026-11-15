# Hooks

Hooking uses MinHook for code hooks and a DXGI vtable swap for render
integration. All hooks are installed in `c_hooks::initialize` and removed
in `c_hooks::destroy`.

## Installed Hooks

| Hook | Target | Purpose |
|------|--------|---------|
| `CreateMove` | `CCSGOInput` vmethod 5 | Game-thread frame entry; caches local pawn/controller and the user command. |
| `MouseInputEnabled` | `CCSGOInput` vmethod 23 | Returns false while the menu is open so the game ignores mouse look. |
| `EnableCursor` | input system vmethod 76 | Forces the cursor free while the menu is open. |
| `FrameStageNotify` | client pattern scan | Stage 7: applies knife/weapon and glove cosmetics. |
| `FireEventClientSide` | client pattern scan | Reads round and purchase notifications to schedule a material refresh. |
| `LevelInit` | client pattern scan | Flags features for re-apply on map load. |
| `Present` | DXGI swap-chain vtable 8 | ImGui frame; renders the menu. |
| `ResizeBuffers` | DXGI swap-chain vtable 13 | Render-target lifecycle on resize. |
| `CreateSwapChain` | DXGI factory vtable 10 | Render-target lifecycle on swap-chain recreation. |

## Address Resolution

DXGI addresses are obtained by creating a temporary device and swap chain
against a hidden window, reading the required vtable slots, and releasing
it. Client functions are located by signature scanning
(`g_opcodes->scan`). Engine event accessors are likewise resolved by
signature.

## Per-Frame Dispatch

- **`Present`**: begin render target → ImGui new frame → `menu::draw` →
  ImGui render → original. The menu and all config file I/O run here.
- **`FrameStageNotify` (stage 7)**: `skin_changer::run` then
  `glove_changer::run`. This is the only place engine entities are
  mutated. Refresh flags set elsewhere are consumed here.

## Input Integrity

The window `WndProc` is subclassed. While the menu is open, input is
delivered to ImGui and game input is suppressed, **except** key and mouse
*release* messages, which are always forwarded to the game so held
movement keys cannot remain logically pressed.

## Teardown

`destroy` restores the cursor state, unhooks the render hooks first,
yields briefly, unhooks the remaining code hooks, releases the DirectX
resources, and uninitializes MinHook. Ordering ensures no hook body is
executing against freed state during unload.
