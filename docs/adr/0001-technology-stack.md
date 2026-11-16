# ADR 0001: Technology stack

- **Status:** Accepted (inherited from `hitokoto`)
- **Date:** 2026-11-16
- **Deciders:** project owner

## 1. Context

`hitokoto-win-updater` is the Windows updater companion for `hitokoto`. To keep the ecosystem
coherent, maintainable, and free of clashing toolchains or glue stacks, it inherits the technology
stack and engineering constraints of the main `hitokoto` application.

## 2. Decision

- **Language & standard:** C++20 (`CMAKE_CXX_STANDARD 20`, `CMAKE_CXX_EXTENSIONS OFF`).
- **UI toolkit:** Qt 6 Widgets (`Core`, `Gui`, `Widgets`, `Network` as needed) under LGPLv3 (dynamic
  linking). No QML, no JavaScript, no web technology (no HTML/CSS/Electron/Tauri/WebView).
- **Build system:** CMake 3.25+ with `CMakePresets.json` (`dev`, `release`, `ci`, `core-sanitize`).
- **Strict layering:**
  - `src/core` uses the C++ standard library only — never Qt, and no platform-specific headers
    directly. All business logic (version comparison, manifest parsing, verification, state machine,
    etc.) lives in `src/core` and is unit-tested without Qt or Windows APIs.
  - `src/ui` and `src/app` provide the thin Qt 6 Widgets UI and application entry point.
  - Any unavoidable Windows-specific operations (e.g. launching/elevating the installer, checking
    running processes, WinHTTP/registry if not covered by Qt) are isolated behind tiny, documented
    interfaces.
- **Testing & quality:**
  - GoogleTest for `src/core` unit tests (buildable and runnable without Qt via `core-sanitize`).
  - Qt Test (`QT_QPA_PLATFORM=offscreen`) for UI tests.
  - Compiler warnings treated as errors in `dev` and `ci` presets (`/W4 /WX` on MSVC;
    `-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion -Wnon-virtual-dtor
    -Wold-style-cast -Woverloaded-virtual -Werror` on GCC/Clang).
  - `clang-format` (`.clang-format`) and `clang-tidy` (`.clang-tidy`, `WarningsAsErrors: '*'`).
- **Licence:** GPL-3.0-or-later (`LICENSE`), using Qt under LGPLv3.

## 3. Dependency policy

- Every library, framework, or toolkit must be well supported and actively maintained.
- No glue stacks; prefer one language (`C++20`) for the whole project.
- No clashing technologies.
- Minimise the dependency footprint; wrap third-party libraries behind small internal interfaces.

## 4. Reconsider if

- A Windows-specific requirement cannot be met cleanly with Qt 6 Widgets + C++20 stdlib + minimal
  Win32 shims; record any exception in a new ADR.
