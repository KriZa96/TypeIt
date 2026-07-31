# TypeIt — Implementation Roadmap

*The phased plan from the current codebase to the application described in
[ARCHITECTURE.md](ARCHITECTURE.md), using a strangler migration: the new core is built
alongside the old app, behaviour is ported with tests green at every step, and the old code is
deleted only once the new frontend reaches parity.*

**Working rule for every phase: the branch builds and all tests pass at every commit.** There
is never a point where the application does not run.

---

## Phase overview

| Phase | Deliverable | Old app still works? |
|---|---|---|
| **0A** | **CI/CD pipeline, built against the code as it stands** | **Yes — no source changes** |
| 0 | Build system, warnings, docs relocation | Yes — unchanged behaviour |
| 1 | `typeit::core` — domain, headless, fully tested | Yes — untouched |
| 2 | `typeit::infra` — SQLite, config, paths, assets | Yes — untouched |
| 3 | `typeit::app` — services over ports | Yes — untouched |
| 4 | `typeit::tui` — new frontend to parity, **old code deleted** | Replaced |
| 5 | History and analytics screens | — |
| 6 | Text library and import pipeline | — |
| 6A | Extended sources: code, subtitles, ebooks, web | — |
| 7 | Endless and Race modes | — |
| 8 | Drills and progression | — |
| 9 | Compatibility pass, packaging, release | — |

Phases 1–3 are strictly additive: the existing application keeps building and running the
whole time. The cutover is a single, reviewable step in Phase 4.

---

## Phase 0A — CI/CD pipeline

**Goal:** a pipeline that tells the truth, built before the code it guards.

Full design in [CI_CD.md](CI_CD.md); issues in
[PHASE-0A-cicd.md](issues/PHASE-0A-cicd.md).

CI-001 – CI-008 land against the **current** codebase, submodule and all, with no source
changes: workflow hardening, a Linux build of the existing code, caching, the first-ever Windows
job, the full matrix, quality and lint checks, sanitizers, and the versioning guards. CI-009 –
CI-018 attach during phases 0–2 as their subjects come into existence.

**Why first.** The repository today has one CI job — Linux, Debug, gcc — and that is precisely
why it can contain three `-Wreorder` violations, a `-Wparentheses` bug, undefined behaviour on
an empty file, and Windows build instructions no machine has ever executed. Building the net
after the rebuild would mean writing 6,000 lines unguarded and then adding a net that has never
caught anything.

**Acceptance**
- Full PR check set under 5 minutes on a warm cache.
- Every guard watched to fail on a deliberate violation.
- Windows built and tested on every push, for the first time in this project's history.
- No workflow holds more permission than it needs; every third-party action SHA-pinned.

**Risk:** the Windows job is expected to fail initially. That failure is the phase's most
valuable output and arrives while the codebase is 1,100 lines rather than 6,000.

---

## Phase 0 — Foundation

**Goal:** make the build tell the truth. No behaviour changes.

Tasks, in an order that keeps the tree building:

1. Restructure CMake: remove hardcoded compilers and the toolchain `set()`; add
   `cmake/CompilerWarnings.cmake`, `Sanitizers.cmake`, `StaticAnalysis.cmake`.
2. Add `cmake/Dependencies.cmake` with `FetchContent` + `FIND_PACKAGE_ARGS` (ADR-007).
3. **Remove the `external/vcpkg` submodule and `.gitmodules`**; keep `vcpkg.json` and
   `vcpkg-configuration.json` as an opt-in path.
4. Add `CMakePresets.json` for Linux (gcc/clang/asan/tidy/coverage) and Windows
   (MSVC/clang-cl).
5. Replace both `file(GLOB)` calls with explicit source lists; drop the duplicated
   `test_input_line_engine.cpp` entry.
6. Extract the current sources into an interim `typeit_legacy` library so tests link it instead
   of recompiling everything.
7. Record the warning baseline, then fix it to zero: the three `-Wreorder` constructors,
   `-Wparentheses` in `Text.cpp:47`, and the sign-conversion warnings across the line engine.
   Fix [C1](CODEBASE_REVIEW.md#4-correctness-defects) (`pop_back` on empty) and
   [C3](CODEBASE_REVIEW.md#4-correctness-defects) (the no-op `ExitLoopClosure`) while here —
   both are one-liners.
8. Add the CI matrix, **including the first Windows job**. Expect it to fail on first run;
   fixing it is part of this phase.
9. Add `.editorconfig`, `.clang-tidy`, and the clang-format CI check.
10. Move documentation to `docs/`, delete the duplicate copies under `src/docs/`, and cut
    `README.md` back to a real README that links here.

**Acceptance**
- `git clone && cmake --preset linux-gcc-debug && cmake --build --preset … && ctest --preset …`
  succeeds with **no submodule step**.
- The same works on Windows with `windows-msvc-debug`.
- Zero warnings with `TYPEIT_WERROR=ON` on gcc, clang, and MSVC.
- All 41 existing tests still pass.
- CI is green on five configurations.

**Risk:** the Windows job may surface real portability problems in the existing code. That is
the point of the phase — better now, when the code is 1,100 lines, than in Phase 9.

---

## Phase 1 — `typeit::core`

**Goal:** the entire domain, headless, with no third-party dependency (ADR-001).

1. `util/`: `Units.h` (strong types), `Result.h`, `IClock.h`, `FakeClock`.
2. `text/`: `Grapheme`, `Segmenter`, `TextBuffer`, `Wrapper`. Table-driven tests including the
   ćčšđž case the README currently documents as a limitation.
3. `session/`: `Keystroke`, `KeystrokeLog`, `TypingModel` with the full state machine and all
   `TypingRules` variants.
4. `metrics/`: `compute`, `timeline`, `key_stats`, `error_map`, `rolling_wpm`, plus the
   `LogBuilder` test DSL and the property tests in
   [TESTING §3.4](TESTING.md#34-metrics--property-tests).
5. `modes/`: `IMode`, `TimedMode`, `WordCountMode`, `QuoteMode`, `ZenMode`.
6. `text_supply/`: `ITextProvider`, `WholeTextProvider`, `ChunkedProvider`.
7. `config/`: the `Config` value type and pure validation.
8. Port every carry-over test from [TESTING §10](TESTING.md#10-regression-tests-carried-over-from-the-current-code).

**Acceptance**
- `typeit_core` links against the standard library only. Verified mechanically: nothing else
  is on its `target_link_libraries`.
- ≥ 90% line coverage on `core`, ≥ 95% on `metrics` and `text`.
- A perfectly typed run reports 100% accuracy **regardless of backspaces** — the property test
  that closes [defect C4](CODEBASE_REVIEW.md#4-correctness-defects).
- WPM matches the definitions in [GAMEPLAY §4](GAMEPLAY.md#4-metrics--exact-definitions),
  cross-checked by hand against three worked examples.
- Zero sleeps in the new tests; `core` tests complete in under two seconds.
- The old application is untouched and still passes its own tests.

**Risk:** grapheme segmentation is the one genuinely fiddly piece. Mitigation: the scope
boundary in [ARCHITECTURE §6.2](ARCHITECTURE.md#62-unicode-scope) is explicit, property tables
are generated rather than hand-written, and the test table is written before the
implementation.

---

## Phase 2 — `typeit::infra`

**Goal:** persistence, configuration, and asset location behind ports.

1. `PlatformPaths` (XDG / Windows) and `AssetLocator` with the documented search path — this
   is where [defect C2](CODEBASE_REVIEW.md#4-correctness-defects), the `__FILE__` asset
   lookup, actually dies.
2. `SqliteDatabase` (RAII connection, prepared-statement cache, `Transaction` guard) and
   `Migrator`; schema v1 from [TECHNICAL §5](TECHNICAL.md#5-database-schema-v1).
3. `SqliteHistoryRepository` and `SqliteTextLibraryRepository`.
4. `TomlConfigStore` with defaults, validation, and key migration.
5. `SystemClock`, `StdFileSystem`.
6. Contract tests against `:memory:`, and temp-directory fixtures driven by `TYPEIT_DATA_DIR`.

**Acceptance**
- Migrations apply cleanly from an empty database and from every prior version; a
  future `user_version` is refused with a clear message rather than a crash.
- A relocated binary finds its assets: build, `cmake --install`, move the tree, run.
- No SQL string concatenation anywhere — every variable is a bound parameter.
- Contract suites pass identically against real and fake implementations.

---

## Phase 3 — `typeit::app`

**Goal:** use cases wired over ports, with fakes standing in for everything external.

1. Port interfaces.
2. `ConfigService`, `SessionService`, `HistoryService`, `TextLibraryService`,
   `ProfileService`.
3. `typeit::cli` argument parsing, plus `--simulate` and `--doctor`.
4. A headless binary that can run a scripted session end to end and print metrics as JSON.

**Acceptance**
- `typeit --simulate script.tks` runs a full session through the real stack, with no terminal,
  and writes a row to a temp database.
- `--stats` and `--export csv|json` work.
- Service tests run entirely on fakes and complete in under a second.
- **A complete typing session is now possible without a single line of FTXUI.** That is the
  proof the layering is real.

---

## Phase 4 — `typeit::tui` and cutover

**Goal:** feature parity with the current application, then delete it.

1. `TerminalApp`, `ScreenStack`, `FrameTicker`.
2. `Theme`, `ThemeLoader`, `ColorQuantizer`, `GlyphSet`, `Capabilities::detect()`.
3. `Keymap` and binding parsing.
4. `TypingArea` (ADR-011 — no `ftxui::Input`), `StatsBar`, `KeyHintBar`.
5. `MenuScreen`, `SessionScreen`, `ResultsScreen`, `HelpScreen`,
   `TerminalTooSmallScreen`.
6. Responsive layout from `Terminal::Size()`; resize re-runs `wrap()` without disturbing the
   session.
7. Render snapshot tests, including the "render twice, model unchanged" guard.
8. **Cutover:** delete `include/`, `src/core/`, `src/view/`, `src/engines/`, `src/main.cpp`,
   the old tests, and the interim `typeit_legacy` target. One commit, reviewable as a whole.

**Acceptance**
- Everything the current application does, the new one does: three bundled difficulties,
  custom file, 15/30/60/custom timers, live WPM and accuracy, per-character colouring,
  restart, and return to menu.
- Plus, immediately: correct metrics, resize support, themes, ASCII fallback, rebindable keys,
  no globals, and history recorded for every run.
- `git grep -l "GameState\|GameOptions\|FocusPosition"` returns nothing.
- Runs correctly in alacritty, kitty, GNOME Terminal, and Windows Terminal at 80×24 and
  120×40.

**Risk:** parity is the phase where scope creep is most tempting. Mitigation: the acceptance
list above is the definition of done. Anything not on it waits for Phase 5+.

---

## Phase 5 — History and analytics

1. `HistoryScreen`: WPM trend chart, mode/date filters, PB table, distribution histogram,
   streaks, totals.
2. `ResultsScreen` enriched: per-second chart with error markers, comparison against average
   and PB, worst pairs, slowest bigrams.
3. `SessionDetailScreen`.
4. Key heatmap widget.
5. `Sparkline` and `Histogram` widgets, and the recent-runs sparkline on the menu.
6. Export to CSV/JSON from the UI.

**Acceptance:** history over 10,000 synthetic sessions renders in under 200 ms with
aggregation done in SQL; every chart degrades correctly to ASCII glyphs and to monochrome.

---

## Phase 6 — Text library

1. Import from file, stdin, and paste; the normalisation pipeline from
   [GAMEPLAY §5.2](GAMEPLAY.md#52-normalisation-at-import).
2. SHA-256 deduplication; tagging; search; delete.
3. Difficulty scoring.
4. Chunking with persistent bookmarks for long documents.
5. `ShuffledSentenceProvider` and `WordPoolProvider`.
6. `TextLibraryScreen`; `--import`, `--list-texts`, `--text`, `--text-id`, and stdin on the CLI.

**Acceptance:** a 500 KB UTF-8 document imports in under a second, is typed across multiple
sessions with the bookmark advancing correctly, and generates a coherent endless word pool.
Invalid UTF-8 is rejected with a byte offset, not a crash.

---

## Phase 6A — Extended text sources

**Goal:** type from anything — code, subtitles, ebooks, web pages, or whatever a converter on
the user's machine can handle.

Design in [TEXT_SOURCES.md](TEXT_SOURCES.md); issues in
[PHASE-6A-text-sources.md](issues/PHASE-6A-text-sources.md).

Phase 6 delivers plain-text import. This phase generalises it into a three-stage pipeline —
acquire, extract, normalise ([ADR-013](ARCHITECTURE.md#adr-013--ingestion-is-a-three-stage-pipeline-separate-from-text-supply))
— and adds formats in four waves:

1. **TX-001 – TX-004** — the fetch/extract split, plus Markdown, source code, and subtitles.
2. **TX-005 – TX-007** — sections, schema v2, and the typing-readiness pass that stops an
   imported chapter from being unpassable.
3. **TX-008 – TX-009** — EPUB, and the external converter hook that inherits pandoc's forty
   formats without shipping a parser
   ([ADR-015](ARCHITECTURE.md#adr-015--heavy-formats-use-an-external-converter-hook-not-a-bundled-parser)).
4. **TX-010 – TX-012** — HTTP fetching and HTML readability extraction
   ([ADR-014](ARCHITECTURE.md#adr-014--network-access-is-optional-at-build-time-and-opt-in-at-runtime)).

Waves 1–2 ship in `2.0.0-beta.1`. Waves 3–4 ship in `2.1.0`, after the 2.0 release — they carry
the only new runtime dependency, the only network surface, and the fuzziest correctness
criterion in the project, and holding them back keeps 2.0 shippable on schedule.

**Acceptance**
- A real EPUB imports with correct chapters, metadata, and readable text.
- Archive-security tests pass: ZIP bomb and path traversal both refused.
- Command injection through the converter hook is impossible by construction.
- A web article imports with the article and none of the navigation.
- `TYPEIT_ENABLE_NETWORK=OFF` produces a binary with no networking symbols.

**Risk:** HTML readability is a heuristic, and JavaScript-rendered pages will fail. Mitigation:
measure against committed fixtures, record where it is poor, and always show a preview — a
silent import of a navigation menu would be worse than refusing.

---

## Phase 7 — Endless and Race

The headline feature, and deliberately late: it depends on history (for adaptive start
speed), on the text library (for endless streams), and on correct metrics (for the accuracy
gate).

1. `EndlessMode` with rolling-window metrics.
2. `Pacer`.
3. `DifficultyController` implementing the ramp law in
   [GAMEPLAY §3.2](GAMEPLAY.md#32-the-ramp-law), with the preset table.
4. `RaceMode`: lives, grace period, catch detection, push-back.
5. Race HUD: pacer bar, inline pacer marker, target speed with trend indicator, lead, lives.
6. Adaptive start speed from `best_sustained_wpm` over 30 days.
7. Speed-wall analysis on the results screen.
8. `peak_wpm` / `wall_wpm` persistence and race personal bests.

**Acceptance**
- All `DifficultyController` property tests pass, including no-gain-below-`A_min`, dead-band
  hysteresis, and recovery after a stumble.
- A scripted player typing at a constant 60 WPM with 99% accuracy converges to a pacer speed
  near 60 and is caught only after a sustained slowdown — verified end to end through
  `--simulate`.
- A second race starts near the first race's peak, demonstrating the progression loop.
- The pacer does not visibly stutter (the reason the dead band exists).

**Risk:** the ramp constants are guesses until someone plays it. Mitigation: every constant is
config-exposed, three presets are shipped, and the tuning pass is an explicit task at the end
of the phase with the `--simulate` harness available to sanity-check curves.

---

## Phase 8 — Drills and progression

1. `DrillService` target selection using the priority function in
   [TECHNICAL §8.4](TECHNICAL.md#84-drill-target-selection).
2. Drill text synthesis — real words containing the target bigrams, with generated fallback.
3. `DrillMode` and its screen.
4. One-key jump from race results into a drill on the speed wall.
5. Daily goal and streak tracking.

**Acceptance:** drills measurably concentrate the targeted bigrams (asserted: target bigram
frequency at least 5× baseline), and the loop race → wall → drill → race works end to end.

---

## Phase 9 — Compatibility, packaging, release

1. Manual pass across the tier-1 terminal matrix in
   [UX §6.1](UX.md#61-support-matrix), on both platforms.
2. `--doctor` finalised, including the key-press tester.
3. Small-terminal and resize edge cases.
4. Install rules, CPack (TGZ/DEB/ZIP), man page, `--help`, `--version`.
5. Release CI on tag push.
6. Fuzz targets; a soak test for endless mode.
7. README with asciinema recordings; final documentation pass against the shipped behaviour.

**Acceptance:** a released artefact installs and runs on a clean Arch box and a clean Windows
box, finds its assets, creates its config and database on first run, and survives a resize
mid-session.

---

## Sequencing rationale

- **Pipeline before build.** Phase 0A first because every later phase — including Phase 0's own
  warning cleanup — is safer when a machine is checking it, and because the pipeline can be
  built against today's code with no source changes at all.
- **Build before code.** Phase 0 next because every subsequent phase benefits from a compiler
  that reports problems, and because the existing code has real warnings today that nothing
  surfaces.
- **Domain before infrastructure.** `core` has no dependencies, so it can be finished and
  fully tested before any decision about SQLite matters. If persistence choices change, `core`
  does not notice.
- **Headless before UI.** By the end of Phase 3 a complete session runs with no terminal. Any
  UI bug found after that is provably a UI bug, which makes the rest of the project easier to
  debug.
- **Parity before features.** Phase 4 ends with the old code deleted and nothing new promised.
  Merging the cutover and new features into one phase is how migrations stall.
- **Race last among the modes.** It consumes history, the text library, and correct metrics.
  Building it earlier would mean building it twice.

## What could go wrong

| Risk | Likelihood | Mitigation |
|---|---|---|
| Grapheme segmentation eats time | Medium | Scope boundary documented; generated tables; tests written first |
| Windows CI reveals real portability problems | High | That is why it lands in Phase 0, at 1,100 lines rather than 6,000 |
| Race constants feel wrong in practice | High | Every constant config-exposed; three presets; explicit tuning task; `--simulate` for curve inspection |
| Phase 4 scope creep delays the cutover | Medium | The acceptance list is the definition of done |
| FTXUI's event model resists custom input handling | Low | ADR-011 removes the dependency on `ftxui::Input`; `Event::character()` already delivers complete UTF-8 sequences |
| Motivation drain on a long plan | Medium | Every phase ends with something runnable and visibly better; phases 5–8 are independently valuable and can be reordered |
