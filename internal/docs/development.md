# Development Guidelines

Engineering conventions and constraints for extending the project safely.

## Invariants

- **Engine mutation is game-thread only.** Cosmetic state may be changed
  only from the `FrameStageNotify` stage-7 path
  (`skin_changer::run` / `glove_changer::run`). Code reached from the
  render/`Present` path (including the menu and config load) must never
  touch engine entities - set a `should_update` flag instead.
- **Persistence is registration only.** Every persistent field must be
  registered in `config_system::setup_values`. Do not read or write
  config files directly elsewhere.
- **Schema gating.** Consumers of the item catalog must check
  `g_item_schema->is_initialized()` and validate indices against current
  list sizes before use.
- **Pointer validation.** Validate engine/interface pointers before
  dereference; the application paths run every frame against changing
  state.

## Conventions

- Naming: `snake_case` for functions/variables, `m_` for members,
  `c_` for classes, `g_` for global singletons.
- Subsystems are global `unique_ptr` singletons declared `inline` in
  their header.
- UI is built exclusively from the panel/list helpers in `menu.cpp`;
  new controls bind directly to config fields.
- Magic layout values are scoped constants within the render scope that
  uses them.

## Adding a Persistent Setting

1. Add the field to the relevant struct in `config.hpp`.
2. Register it in `config_system::setup_values` with a stable
   category/key and sensible default.
3. Bind a control to it via the menu helpers.
4. If it affects cosmetics, ensure the owning feature re-applies when its
   `should_update` flag is set.

## Adding a Hook

1. Declare the trampoline/hook in `hooks.hpp`.
2. Install in `c_hooks::initialize`; unhook in `c_hooks::destroy` in an
   order that drains render hooks before code hooks.
3. Keep hook bodies minimal; defer work to the appropriate thread.

## Build & Validate

```
msbuild vc.sln -t:Build -p:Configuration=Release -p:Platform=x64 -m
```

Output: `x64/Release/CS.dll`. Validate by reinjecting; the previous image
must be unloaded first.

## Known Limitations

- Stored knife/glove/paint-kit selections are catalog **indices**; an
  economy catalog change can shift what an index resolves to. Selections
  are bounds-checked but not name-pinned.
- The item catalog depends on the economy schema being initialized; until
  then lists display a loading state.
- Steam persona resolution is best-effort (most-recent profile / registry
  fallback) and display-only.
- Configuration files are plain JSON and not encrypted.
