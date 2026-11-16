# Story backlog

- **Status:** Active
- **Date:** 2026-11-16
- **Basis:** `docs/adr/0001-technology-stack.md` (C++20, Qt 6 Widgets, CMake)

Each feature is one branch/PR and may hold several closely related stories (more only after asking);
stories are implemented test-first. Tests reference the story ID (see "Conventions").

| Order | ID | Story | Depends on | Size | Status |
|---|---|---|---|---|---|
| 1 | [S000](000-repository-skeleton.md) | Repository skeleton, build and CI | – | M | In review |
| 2 | [S001](001-cli-updater.md) | CLI update checker and MSI installer | S000 | M | In review |

Sizes: S = up to a day, M = a few days, L = about a week of part-time work (rough).

Status: **Open** → **Confirmed** (by the user) → **In progress** → **In review** (CI green,
checks pass) → **Done** (the user has accepted the functionality; only the user sets it).

## Decisions

1. **Licence:** GPL-3.0-or-later (`LICENSE`). Qt is used under LGPLv3 (dynamic linking).
2. **Minimum platforms:** Windows 10 22H2 and 11 (x64). `src/core` remains pure C++20 stdlib so it
   can also be built and tested with sanitizers on Linux.

## Conventions

- Story files: `docs/stories/NNN-title.md`, written before implementation and confirmed by the user.
  The confirmation may happen in conversation; it need not show in the commits.
- Review records (Advocatus Diaboli): `docs/reviews/NNN-title-<mode>.md`, the objections as
  delivered plus the disposition of each; the story links nothing, the file name carries the ID.
- Points that are not specified yet go to `docs/stories/unrefined.md`, on whatever branch they are
  noticed; a refined entry becomes a story and leaves that file.
- Scope changes: `docs/brief.md` (if present) stays the user's brief. Extensions become new stories
  here, with their reason.
- Core tests (GoogleTest): test suite named after the story, e.g. `TEST(S001_Version, ...)`.
- UI tests (Qt Test, `QT_QPA_PLATFORM=offscreen`): one test class per story, e.g. `tst_s001_...`.
- A story is ready for review when its scenarios pass on CI, nothing regressed, and formatting/linting
  checks pass. It is done when the user has accepted it.
