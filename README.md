# hitokoto-win-updater

> Updating is task of the package manager, Microsoft!

Windows updater companion for [`hitokoto`](https://github.com/Daniel-Fuerst/hitokoto).

## CLI Usage

```sh
hitokoto-win-updater-cli                # check installed version, download & install newer MSI if available
hitokoto-win-updater-cli --check        # only check and report versions
hitokoto-win-updater-cli --silent       # run msiexec silently (/qn)
hitokoto-win-updater-cli --yes          # install without prompting if hitokoto is not yet installed
hitokoto-win-updater-cli --version      # print updater version
```

How it works:
1. Detects the currently installed `hitokoto` version and install scope (`per-user` or `per-machine`)
   from the Windows Uninstall registry entries (with `hitokoto.exe --version` as a fallback).
2. Queries the latest stable release from GitHub (`Daniel-Fuerst/hitokoto`).
3. If a newer version is available, downloads `hitokoto-<version>-x64.msi` and `SHA256SUMS` to a
   temporary directory, verifies the SHA-256 checksum, and runs `msiexec.exe` while preserving the
   existing install scope.

## Building

Requires a C++20 compiler, CMake 3.25+, and Qt 6.8+ (`Core`, `Network`; `Test` for tests).

```sh
# Development build & tests
cmake --preset dev
cmake --build --preset dev
ctest --preset dev

# Core library & unit tests only with sanitizers (no Qt required, Linux/macOS/Clang/GCC)
cmake --preset core-sanitize
cmake --build --preset core-sanitize
ctest --preset core-sanitize
```

On Windows (or with a custom Qt install), pass `-DCMAKE_PREFIX_PATH=C:/path/to/Qt` when configuring.

## Documentation

- [`CLAUDE.md`](CLAUDE.md) — Working agreement for coding agents
- [`docs/adr/`](docs/adr/) — Architecture decision records
- [`docs/stories/`](docs/stories/) — Story backlog and acceptance scenarios

## License

GPL-3.0-or-later (see [`LICENSE`](LICENSE)). Uses Qt under the LGPLv3.
