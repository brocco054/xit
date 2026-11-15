# Companion mode

## Runtime boundary

`ui/` is the supported standalone application. It uses local browser storage, catalog metadata, reference images and Canvas rendering. It never communicates with `cs2.exe`.

`internal/` contains a legacy native DLL and loader. Those components are intentionally outside the companion startup and export path. They modify client-side game state and remain restricted to the project's local `-insecure` laboratory workflow.

## Portable export contract

The workbench exports a JSON document with:

- `format`: `hy3x.acervo.collection`
- `version`: `1`
- `source`: `hy3x-acervo-workbench`
- `items`: one selected finish per weapon, knife or glove slot, with catalog IDs, display labels, seed and wear

This document is a portable collection record, not a runtime configuration. It contains no native changer sections or enable flags and is not loadable by the legacy DLL.

## Compatibility statement

Running the companion does not alter the game process. This is a process-isolation property, not a claim that the legacy skin changer is VAC-compatible. The companion previews cosmetics only; it does not apply them to a live match, inventory or game client.
