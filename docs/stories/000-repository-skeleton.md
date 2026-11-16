# S000 — Repository skeleton, build and CI

- **Status:** In review · **Order:** 1 · **Depends on:** –

## Story

As a developer, I want a CMake/C++20/Qt 6 repository skeleton with presets, strict core layering,
GoogleTest, formatting, linting and CI, so that every feature can be built and verified test-first
on Linux and Windows.

## Acceptance scenarios

**Scenario: core builds and tests without Qt under sanitizers**
- Given a checkout of the repository
- When `cmake --preset core-sanitize && cmake --build --preset core-sanitize && ctest --preset core-sanitize` runs
- Then `src/core` compiles with C++20 stdlib only (no Qt, no Windows headers) and all core unit tests pass

**Scenario: full build and tests pass in dev preset**
- Given a checkout with Qt 6 (Core, Network, Test)
- When `cmake --preset dev && cmake --build --preset dev && ctest --preset dev` runs
- Then the core, platform, CLI and test targets compile with warnings as errors and all tests pass

**Scenario: formatting and static analysis pass**
- Given all `.cpp` and `.hpp` files tracked by git
- When `clang-format --dry-run --Werror` and `clang-tidy` run
- Then zero formatting or lint errors are reported

**Scenario: version is derived from git tags**
- Given `scripts/version/UpdaterVersion.cmake`
- When CMake configures on a tagged commit, an untagged commit, or with `-DUPDATER_VERSION=X.Y.Z`
- Then the full and core SemVer strings follow ADR 0010 rules
