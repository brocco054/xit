# Hy3x Acervo Companion

Hy3x Acervo Companion is a standalone browser-based catalog, cosmetic preview and preset organizer for Counter-Strike 2 finishes.

## What it does

- Browse the curated finish catalog and its reference images.
- Preview illustrative wear and pattern variations in a local canvas.
- Save, duplicate, compare and back up presets in browser storage.
- Export a portable JSON collection using the `hy3x.acervo.collection` schema.

## Run

Open `ui/index.html` in a modern browser. For a local HTTP origin, run `python -m http.server 8080` from the repository root and visit `http://localhost:8080/ui/`.

## Separation from the native laboratory module

The companion is independent of the CS2 process. It does not inject a DLL, open the game process, install hooks, read or write game memory, load a driver, or change in-game items. Its Canvas preview is illustrative only.

The native code under `internal/` is a separate legacy laboratory module. It changes client-side game state, requires a local `-insecure` session, and is not invoked, built, or loaded by the companion. No VAC or official matchmaking compatibility is claimed. Removing the `-insecure` guard or changing the loader would not make that module suitable for official matchmaking.

The JSON collection export is for backup and sharing by companion tools. It is deliberately distinct from the native module's `.cfg` format and is not accepted by the native loader.

## Build a standalone package

On Windows with PowerShell and Node.js installed, run this from the repository root:

```powershell
.\tools\build-companion.ps1 -Version 1.0.0
```

The script runs the companion tests and creates a ZIP containing only the browser app, its static assets, the companion documentation, and a SHA-256 manifest. It rejects `.exe`, `.sys`, `.cfg`, `CS.dll`, and native loader artifacts in the companion payload.

## Tests

Run the portable export tests with Node.js:

```sh
node --test --test-isolation=none tests/profile-export.test.cjs
```
