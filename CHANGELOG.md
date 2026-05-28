# Changelog

## [1.0.2.release] - 2026-05-28

### Added
- Rebranding to **TinyRdp** with first-run migration support for legacy `%AppData%`, `%TEMP%`, and Credential Manager prefixes.
- Folder context menu actions: **Expand** and **Collapse**.
- Live drag-and-drop destination highlighting while moving tree items.
- New GitHub Actions workflow (`.github/workflows/build.yml`) for Windows CMake/MSVC build validation.
- Example mRemoteNG export template added at `examples/mremoteng-export-example.xml`.
- README enhancements with professional GitHub layout, app screenshot, icon assets, and Open Graph branding image.
- New MIT `LICENSE` file.

### Changed
- Main window now enforces a minimum size of **200x200**.
- Root **Connections** folder can now be renamed with validation (1-32 chars, alphanumeric only).
- Drag/drop behavior updated so root cannot be dragged, but dropping items onto root remains supported.
- README build guidance cleaned up (removing build-script/build-icon emphasis and replacing with polished project documentation).

### Fixed
- Connecting no longer fails with raw **"Element not found"** when a linked Credential Manager password entry has been deleted; connection proceeds and prompts as needed.
- `builder.ps1` now handles stale CMake cache/source-path mismatches after repository rename.
- Changelog version labeling corrected to `1.0.2.release`.
- Changelog formatting to remove extra indentations

## [1.0.1.release] - 2026-05-27

### Added
- Bulk credential assignment for multi-selected sessions and from folder context (**Set Credential...** with direct children or subtree scope).
- In-app release notes dialog via **File -> Changelog...**.
- Theme selection via **File -> Theme** (**System**, **Light**, **Dark**).
- Alt+right-click range selection for visible sessions.

### Fixed
- Session, folder, and credential dialogs: Save/Cancel buttons no longer clipped; layout refreshes after the window is sized.
- Plain left-click now clears a previous multi-selection correctly.
- Credential selector in assignment flows refreshes correctly without opening **Manage...** first.
- Window title no longer includes the version number (version remains in About and the status bar).
