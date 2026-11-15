# UI System

The UI is a single Dear ImGui window rendered every frame from the DXGI
`Present` hook. It is purely presentational: every control binds directly
to a configuration field or item-schema list.

## Window

- Fixed size, centered on first appearance, no native title bar
  (`NoTitleBar | NoResize | NoCollapse | NoScrollbar`).
- Custom-painted top and bottom strips with rounded outer corners that
  match the window radius.
- Layout: **top bar** (brand + tabs) → **body** (three panels) →
  **bottom bar** (status).

## Theme

Initialized once in `setup_style` and reapplied after a font-atlas
rebuild. Flat, dark, single accent.

| Token            | Value      | Use                                 |
|------------------|------------|-------------------------------------|
| Accent           | `#c8f03a`  | Active tab, selected list item      |
| Confirm          | `#4caf50`  | Enabled checkbox check mark         |
| Window bg        | `#1a1a1f`  | Window base                         |
| Bar bg           | `#1e1e26`  | Top / bottom strips                 |
| Panel bg         | `#1c1c24`–`#1e1e26` | Per-column panels          |
| Divider          | `#2a2a35`  | Separators, borders                 |

Rounding: window 8 px, child panels 6 px, frames/inputs/buttons 4 px,
list selection 4 px.

## Fonts

`rebuild_fonts` builds the atlas at the active DPI scale, preferring
Segoe UI Semibold, then Segoe UI, then the ImGui default. The result is
set as `io.FontDefault` and applied globally; the menu pushes it for the
whole frame.

## Tab System

A fixed tab set - **Config, Knife, Glove, Skin** - rendered as
right-aligned text buttons in the top bar. The active tab uses the accent
color; the selected index dispatches to one panel renderer per frame.

## Panel Layout

Each gameplay tab uses an identical three-column structure built from
shared helpers:

| Column | Knife / Glove / Skin                      |
|--------|-------------------------------------------|
| Left   | Settings (enabled, wear, seed, name)      |
| Middle | Model / weapon list                       |
| Right  | Paint kit / finish list                   |

The Config tab uses: profile list · selected profile actions · profile
management actions.

## Reusable Helpers

- `begin_panel` / `end_panel` - bordered, padded column with a section
  header; per-column background color.
- `vc_list` - scrollable selectable list bound to an `int*` index.
- `vc_selectable` - a selectable with a manually drawn rounded highlight
  (selected and hover), accent text when selected, fixed left text indent.
- `section_header`, `field_label`, `tab_button`, `column_widths` -
  consistent typography and spacing.

All list and panel rendering goes through these helpers so every tab is
visually and behaviorally consistent.

## Status Bar

The bottom strip renders the resolved Steam persona name, right-aligned.
If no name resolves, nothing is drawn. Display only.

## Input Ownership

While the menu is open, the WndProc subclass routes input to ImGui and
suppresses game input, but key/button **release** messages are still
forwarded to the game so movement state cannot latch. `MouseInputEnabled`
and `EnableCursor` are overridden to free the cursor for the menu.
