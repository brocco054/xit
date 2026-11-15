# Architecture

## Design Principles

- **Single attach thread** owns startup and shutdown ordering.
- **Engine-safe application**: all cosmetic mutation occurs on the game
  thread during `FrameStageNotify` stage 7. The render thread only draws UI.
- **Global singletons** via `inline const auto g_* = std::make_unique<…>()`
  for each subsystem; no manual lifetime juggling.
- **Registration-based persistence**: config fields self-describe; the
  serializer has no knowledge of individual settings.

## Subsystems

### Core (`entry.cpp`)
Hosts `DllMain` and the attach thread (`start_address`). Performs ordered
initialization and the terminate watch (`VK_END`), then runs `destroy`.

### Modules / Interfaces / Schema (`valve/`)
- `modules` resolves loaded game module bases.
- `interfaces` resolves engine/client interface pointers.
- `schema` resolves netvar/class field offsets.
- `vtables/` holds protobuf message types and interface vtable wrappers.

### Item Schema (`features/shared/item_schema`)
Builds the catalog of knives, gloves, weapons and their valid paint kits
from the game's `EconItemSchema`. Entries are de-duplicated by display
name and ordered for presentation. Construction is idempotent and retried
per frame until the economy data is available; `is_initialized()` gates
all consumers.

**Identity contract.** List positions are presentation-only and unstable
(map-walk + filter + dedup + sort). Stable identity is the engine
*definition index* and *paint-kit id*. Index 0 of `knives`/`gloves` is a
synthetic `(None)` sentinel; weapons have none (they key by definition
index). The UI panel and the runtime appliers resolve a selection through
the *same* `def_from_index`, and configs persist the stable ids
(`*_id` keys) and resolve them back locally. No persisted value or runtime
decision keys off list position. Full record:
[`glove_system.md`](glove_system.md), [`skin_pipeline.md`](skin_pipeline.md).

### Hooks (`hooks/`)
Installs and dispatches:
- `CreateMove` - game-thread frame entry.
- `MouseInputEnabled` / `EnableCursor` - input ownership while the menu is open.
- `FrameStageNotify` - cosmetic application (stage 7).
- `FireEventClientSide` - event-driven refresh + knife kill-feed rename.
- `LevelInit` - re-apply on map load.
- `Present` / `ResizeBuffers` / `CreateSwapChain` - render integration.

### Render (`directx/`)
Creates a throwaway swap chain to resolve the DXGI vtable addresses,
subclasses the game window's `WndProc`, initializes the ImGui DX11/Win32
backends, and tracks DPI scale to rebuild the font atlas.

### UI (`menu/`)
Owns the ImGui theme and font atlas and renders the tabbed control panel.
Stateless with respect to gameplay - it reads and writes config fields only.

### Features (`features/`)
- `skin_changer` - knives and weapons (shared system; one update flag).
- `glove_changer` - gloves (frame-window apply state machine).
- `shared/econ_item_attribute_manager` - econ attribute construction.
- `shared` `c_hud` - HUD weapon-icon invalidation and skin regeneration.

### Config (`sdk/config_system/`)
JSON persistence keyed by registered pointers. See
[`config.md`](config.md).

## Subsystem Relationships

```
entry ──> modules ──> interfaces ──> schema
   │                        │
   │                        ├──> item_schema ──┐
   │                        │                  │
   ├──> config_system <─────┘            features (skin/glove)
   │                                           ▲
   ├──> directx ──> ImGui backend              │
   │        ▲                                  │
   └──> hooks ──┬── Present ──> menu ──> config fields
                └── FrameStageNotify ──> features ──> engine entities
```

## Execution Flow

1. **Attach** - ordered init on the spawned thread.
2. **Per frame (render thread)** - `Present` → ImGui new frame →
   `menu::draw` → ImGui render.
3. **Per frame (game thread)** - `FrameStageNotify(7)` →
   `skin_changer::run` → `glove_changer::run`.
4. **Events** - `FireEventClientSide` / `LevelInit` set refresh flags
   consumed by the next stage-7 pass.
5. **Shutdown** - `destroy` removes hooks and releases resources.

## Threading Model

| Thread        | Responsibilities                                  |
|---------------|---------------------------------------------------|
| Attach thread | Init, terminate watch, shutdown                   |
| Render thread | DXGI Present, ImGui, menu, config save/load I/O   |
| Game thread   | CreateMove, FrameStageNotify, cosmetic application |

Cosmetic application is confined to the game thread; the render thread
never mutates engine entities.
