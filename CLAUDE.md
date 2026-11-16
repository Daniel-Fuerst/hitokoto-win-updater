# Working agreement for coding agents

Read before any work: `docs/adr/` (accepted decisions) and `docs/stories/README.md` (backlog and order).
If `docs/brief.md` exists, read it as well.

- Stack (ADR 0001): C++20, Qt 6 Widgets, CMake 3.25+. No QML, no JavaScript, no web technology.
- `src/core` uses the C++ standard library only, never Qt (and no Windows-only headers directly; isolate
  any OS-specific shims behind small interfaces). All logic lives in `src/core` and is unit-tested.
- One branch per feature, in backlog order. A branch may hold several closely related stories;
  anything beyond that only after asking.
- Only implement stories the user has confirmed. Confirmation may happen in conversation; it need
  not show in the commits. Test first (red, green, refactor); tests carry the story ID.
- Scope changes become new stories in `docs/stories/` with their reason; `docs/brief.md` stays the
  user's brief.
- A story is ready for review when its scenarios pass on CI, formatting and static analysis checks
  pass: set it to "In review" and summarise what changed, which tests were added, and what is next.
  It is done only when the user accepts the functionality; the user marks it "Done".
- Ask before decisions that are expensive to reverse or conflict with existing ADRs/brief; record
  them as ADRs in `docs/adr/`.
- New dependencies need a justification: well supported and actively maintained, no glue stacks, no
  clashing technologies, minimal dependency footprint. Wrap third-party libraries behind small
  interfaces.
- Versions: changes reach `main` only through pull requests, also notes and status changes. Every
  pull request into `main` carries exactly one label `version:major`, `version:minor`,
  `version:patch` or `version:none` (anything that ships in the program is at least
  `version:patch`); the label is part of the review and must fit the pull request's final content.
  The version comes only from the tag `vMAJOR.MINOR.PATCH` that `version.yml` sets after the merge:
  no `VERSION` in `project()`, no `v*` tag by hand. Builds take it from `git describe` at configure
  time, so a build keeps the version of its last configure.
- Core tests use GoogleTest (`TEST(SXXX_StoryName, ScenarioName)`). UI tests use Qt Test with
  `QT_QPA_PLATFORM=offscreen` (`tst_sxxx_...`).

Build and check:

```sh
cmake --preset dev && cmake --build --preset dev && ctest --preset dev
clang-format --dry-run --Werror $(git ls-files '*.cpp' '*.hpp')
```
