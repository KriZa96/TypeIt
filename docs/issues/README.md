# TypeIt — Issue Backlog

**171 active issues** taking TypeIt from **1.0.0** (today's application) to **2.0.0** (the
rebuild), plus the extended text sources that land in **2.1.0**. Each is scoped to be finishable
in one sitting to a few days, and each carries its own test requirements and acceptance
criteria.

(141 `TI-` + 18 `CI-` + 12 `TX-`. Four `TI-` ids are superseded and retained only so existing
cross-references resolve.)

| Phase | File | Issues | Milestone |
|---|---|---|---|
| **0A — CI/CD** ← **start here** | [PHASE-0A-cicd.md](PHASE-0A-cicd.md) | CI-001 – CI-018 | `v2.0.0-alpha.0` |
| 0 — Foundation | [PHASE-0-foundation.md](PHASE-0-foundation.md) | TI-001 – TI-022 | `v2.0.0-alpha.1` |
| 1 — Core domain | [PHASE-1-core.md](PHASE-1-core.md) | TI-023 – TI-052 | `v2.0.0-alpha.2` |
| 2 — Infrastructure | [PHASE-2-infra.md](PHASE-2-infra.md) | TI-053 – TI-065 | `v2.0.0-alpha.3` |
| 3 — Application + CLI | [PHASE-3-app.md](PHASE-3-app.md) | TI-066 – TI-078 | `v2.0.0-alpha.4` |
| 4 — TUI + cutover | [PHASE-4-tui-cutover.md](PHASE-4-tui-cutover.md) | TI-079 – TI-098 | `v2.0.0-alpha.5` |
| 5 — History | [PHASE-5-history.md](PHASE-5-history.md) | TI-099 – TI-109 | `v2.0.0-alpha.6` |
| 6 — Text library | [PHASE-6-text-library.md](PHASE-6-text-library.md) | TI-110 – TI-119 | `v2.0.0-beta.1` |
| 6A — Extended text sources | [PHASE-6A-text-sources.md](PHASE-6A-text-sources.md) | TX-001 – TX-012 | `beta.1` / `2.1.0` |
| 7 — Endless + Race | [PHASE-7-endless-race.md](PHASE-7-endless-race.md) | TI-120 – TI-129 | `v2.0.0-beta.2` |
| 8 — Drills | [PHASE-8-drills.md](PHASE-8-drills.md) | TI-130 – TI-134 | `v2.0.0-rc.1` |
| 9 — Release | [PHASE-9-release.md](PHASE-9-release.md) | TI-135 – TI-145 | `v2.0.0` |

Versioning policy: [VERSIONING.md](../VERSIONING.md). Phase rationale and risks:
[ROADMAP.md](../ROADMAP.md). Pipeline design: [CI_CD.md](../CI_CD.md).

---

## Execution order

```
Phase 0A  CI-001 … CI-008      pipeline, against the code as it stands today
   ↓
Phase 0   TI-001 … TI-022      build system, warnings, docs
   ↓
Phase 1   TI-023 … TI-052      core domain            ┐
Phase 2   TI-053 … TI-065      infrastructure         ├─ CI-009 … CI-018 land alongside,
Phase 3   TI-066 … TI-078      services + CLI         ┘  as their subjects come to exist
   ↓
Phase 4   TI-079 … TI-098      TUI, then CUTOVER
   ↓
Phase 5   TI-099 … TI-109      history
Phase 6   TI-110 … TI-119      text library
Phase 6A  TX-001 … TX-007      pipeline + formats     (waves 1–2, ship in beta.1)
   ↓
Phase 7   TI-120 … TI-129      endless + race
Phase 8   TI-130 … TI-134      drills
Phase 9   TI-135 … TI-145      release  →  v2.0.0
   ↓
Phase 6A  TX-008 … TX-012      EPUB, converters, web  (waves 3–4, ship in 2.1.0)
```

**Phase 0A comes first.** The pipeline is built before the code it guards, against the current
vcpkg-submodule build, so the rebuild is never written without a net. Rationale in
[CI_CD §1](../CI_CD.md#1-why-this-comes-first).

### On the id prefixes

`TI-` was allocated before the CI and text-source workstreams existed. Rather than renumber 145
issues and risk breaking hundreds of cross-references, the two new workstreams get their own
prefixes — `CI-` and `TX-`. **Ids are identifiers, not a schedule**; the order above is the
schedule, and the per-issue `Depends on` fields are authoritative.

---

## Definition of done

An issue is closed only when **every** line below is true. This list is the contract; the
per-issue acceptance criteria are additional, not alternative.

1. The acceptance criteria in the issue are all ticked.
2. **Unit tests exist for every behaviour the issue introduces**, including the failure paths
   and the boundary cases. New code in `typeit::core` has no untested branch — there are no
   dependencies to blame there.
3. A bug fix has a test that **fails before the fix and passes after**. Verify that ordering
   explicitly; a test written after the fix that has never been seen to fail proves nothing.
4. The full suite passes on the whole CI matrix: Linux gcc, Linux clang, Windows MSVC, plus
   ASan + UBSan.
5. Zero warnings with `TYPEIT_WERROR=ON` on all three compilers.
6. `clang-format` clean; no new `clang-tidy` findings.
7. No test sleeps. Time is injected through `IClock`.
8. No global mutable state introduced.
9. `CHANGELOG.md` updated under `## [Unreleased]` if behaviour changed, referencing the issue
   id.
10. Documentation in `docs/` updated if the issue changed anything a document describes.
11. Coverage thresholds for the layer still met ([TESTING §9](../TESTING.md#9-coverage)).

### On testing specifically

The testing bar is the highest-leverage thing in this backlog, so it is worth being explicit
about what "as much as possible" means per layer:

| Layer | Bar |
|---|---|
| `core` | ≥ 90% overall, ≥ 95% for `metrics` and `text`. Pure code, no excuses. Property tests where a property exists |
| `app` | ≥ 85%. Every service failure path exercised through an injected adapter failure |
| `infra` | ≥ 75%. Real in-memory SQLite, never a mock of the SQL |
| `tui` | ≥ 60%. Every widget gets a snapshot test **and** the render-purity assertion |

Three properties are load-bearing and appear as named tests, because they are the defects that
motivated the rebuild:

- **Accuracy is 100% for a perfectly typed run regardless of backspaces** (TI-037 — closes
  defect C4)
- **WPM matches the standard definitions with unclamped elapsed time** (TI-036 — closes defect
  C5)
- **`Render()` mutates nothing** (TI-087 — closes the render-side-effect pattern)

---

## Issue anatomy

```markdown
## TI-042 — Short imperative title

**Type** feat · **Size** M · **Priority** P1 · **Depends on** TI-033, TI-035 · **Docs** [link]

One paragraph: what and, where it is not obvious, why.

**Scope**
- In: what this issue delivers
- Out: what it deliberately does not, and where that lives instead

**Unit tests** (`FileTest.cpp`)
- Specific, named cases — not "add tests"

**Acceptance**
- [ ] Checkable statements
```

`Type` ∈ `feat`, `fix`, `refactor`, `test`, `docs`, `build`, `ci`, `perf`, `chore` — matching
the Conventional Commit prefixes in [STYLE §11](../STYLE.md#11-commits-and-branches).

`Size` — `XS` under 2 h · `S` half a day · `M` one to two days · `L` three to five days.
Anything larger is split before work starts.

`Priority` — `P0` blocks the phase · `P1` required for the milestone · `P2` should ship ·
`P3` nice to have, first to be cut.

---

## Workflow

```bash
git switch v2
git switch -c feat/TI-042-typing-model
# work, with tests
cmake --build --preset linux-gcc-debug && ctest --preset linux-gcc-debug
cmake --build --preset linux-clang-asan && ctest --preset linux-clang-asan
# PR titled: feat(core): implement typing model state machine (TI-042)
```

Branch names are `<type>/TI-###-slug`. Commit subjects follow Conventional Commits and
reference the issue. One logical change per commit; refactors are separate commits from
behaviour changes.

---

## Suggested labels

`phase:0`…`phase:9` · `type:feat|fix|refactor|test|docs|build|ci|perf|chore` ·
`layer:core|app|infra|tui|cli|build` · `size:xs|s|m|l` · `priority:p0|p1|p2|p3` ·
`closes-defect` (the issues that fix a `CODEBASE_REVIEW.md` finding) ·
`blocked` · `good-first-issue`

---

## Critical path

The chain that gates everything else. Slippage here slips the release; the rest can be
resequenced within its phase.

```
CI-001 scaffolding → CI-002 linux build → CI-003 caching → CI-004 WINDOWS → CI-005 matrix
                                                                    ↓
TI-003 warnings → TI-004 baseline ─────────────────────────────→ TI-020 -Werror
TI-007 deps → TI-008 no submodule → TI-010 presets
TI-002 version → CI-008 version guards
TI-023 core target → TI-027 grapheme → TI-028 segmenter → TI-030 TextBuffer
                                                        → TI-033 TypingModel
                                                        → TI-036/037 metrics
TI-056 sqlite → TI-057 migrator → TI-058 history repo
TI-068 SessionService → TI-075 --simulate        ← "a session runs with no terminal"
TI-087 TypingArea → TI-092 SessionScreen → TI-098 parity → TI-097 CUTOVER
TI-121 Pacer → TI-122 DifficultyController → TI-124 RaceMode → TI-126 adaptive start
TX-001 fetch/extract split → TX-005 sections → TX-006 schema v2 → TX-008 EPUB
```

Four issues are worth flagging as the ones most likely to hurt:

- **CI-004** (first Windows job) — deliberately scheduled at 1,100 lines rather than 6,000.
  Expect it to fail on the first run; that failure is the point.
- **TI-028** (grapheme segmentation) — the fiddliest code in the project. Scope boundary is
  documented; write the test table before the implementation.
- **TI-097** (cutover) — gated on TI-098's written parity checklist. Do not start it early and
  do not merge features into it.
- **TX-011** (HTML readability) — the only component whose correctness criterion is a
  judgement call. Measure it against fixtures, record where it is poor, and fail honestly
  rather than importing a navigation menu.

---

## Issues that close a review defect

Direct traceability from [CODEBASE_REVIEW.md](../CODEBASE_REVIEW.md) findings to the issue that
fixes each one:

| Finding | Issue |
|---|---|
| C1 — `pop_back` on empty file (UB) | TI-005 |
| C2 — `__FILE__` asset lookup, non-relocatable binary | TI-055, verified again in TI-137 |
| C3 — no-op `ExitLoopClosure` | TI-006 |
| C4 — accuracy unrecoverable after backspace | TI-037 |
| C5 — non-standard WPM on clamped time | TI-036 |
| C6 — `-Wparentheses` in `Text.cpp:47` | TI-004 |
| C7 — signed/unsigned at container boundaries | TI-004, TI-024, TI-033 |
| C8 — `-Wreorder` × 3 | TI-004 |
| C9 — session constructed at startup | TI-092 |
| §3.1 — global mutable state | TI-081, TI-091, TI-097 |
| §3.2 — model updated inside render | TI-087, TI-092 |
| §3.3 — FTXUI in the domain | TI-023, TI-031 |
| §3.4 — byte-oriented text model | TI-027, TI-028 |
| §3.5 — reference plumbing | TI-024, TI-068 |
| D1–D11 — build and tooling | TI-003 – TI-020 |
| §6 — sleeping, order-dependent tests | TI-026, TI-044, TI-080 |
| T1 — undefined terminal macro | TI-082 |
| T2 — no colour fallback | TI-084 |
| T3 — fixed layout sizes | TI-089, TI-090 |
| T4 — `Ctrl+T` binding | TI-086 |
| T5 — unverified Windows console | TI-096, TI-017 |
| T6 — `ftxui::Input` misuse | TI-087 |
| §8 — duplicated documentation | TI-021 |

---

## Full backlog

`✓` in the *Def* column marks an issue that closes a documented defect.

| ID | Title | Ph | Type | Sz | Pr | Depends | Def |
|---|---|---|---|---|---|---|---|
| CI-001 | Workflow scaffolding and hardening | 0A | ci | S | P0 | — | |
| CI-002 | Build and test the current code on Linux | 0A | ci | S | P0 | CI-001 | ✓ |
| CI-003 | Composite setup action and caching | 0A | ci | M | P0 | CI-002 | |
| CI-004 | Windows job | 0A | ci | L | P0 | CI-003 | ✓ |
| CI-005 | Full matrix and aggregator check | 0A | ci | M | P0 | CI-004 | ✓ |
| CI-006 | Quality workflow | 0A | ci | M | P0 | CI-005 | ✓ |
| CI-007 | Sanitizer workflow | 0A | ci | S | P0 | CI-005 | |
| CI-008 | Version guards and derivation | 0A | ci | M | P0 | CI-005,TI-002 | |
| CI-009 | Coverage workflow | 0A | ci | M | P1 | CI-005,TI-023 | |
| CI-010 | Security and supply chain | 0A | ci | M | P1 | CI-005 | |
| CI-011 | Release workflow | 0A | ci | L | P1 | CI-008,TI-138 | |
| CI-012 | Version bump workflow | 0A | ci | S | P2 | CI-008 | |
| CI-013 | Nightly workflow | 0A | ci | L | P2 | CI-005 | |
| CI-014 | Benchmarks and regression tracking | 0A | ci | M | P2 | TI-039 | |
| CI-015 | Fuzzing workflow | 0A | ci | M | P2 | TI-141 | |
| CI-016 | Documentation site | 0A | ci | S | P3 | CI-006 | |
| CI-017 | PTY smoke test | 0A | ci | M | P1 | TI-092,CI-005 | ✓ |
| CI-018 | Repository automation and protection | 0A | ci | S | P2 | CI-005 | |
| TI-001 | Tag 1.0.0 and create CHANGELOG | 0 | chore | XS | P0 | — | |
| TI-002 | Single-source version and generated `Version.h` | 0 | build | S | P1 | 001 | |
| TI-003 | `cmake/CompilerWarnings.cmake` | 0 | build | S | P0 | — | |
| TI-004 | Clear the warning baseline to zero | 0 | fix | M | P0 | 003 | ✓ |
| TI-005 | Fix `pop_back` on empty file | 0 | fix | XS | P1 | — | ✓ |
| TI-006 | Fix no-op `ExitLoopClosure` | 0 | fix | XS | P3 | — | ✓ |
| TI-007 | `Dependencies.cmake` with FetchContent | 0 | build | M | P0 | — | ✓ |
| TI-008 | Remove the vcpkg submodule | 0 | build | S | P0 | 007 | ✓ |
| TI-009 | Remove hardcoded compilers | 0 | build | XS | P0 | 007 | ✓ |
| TI-010 | `CMakePresets.json` | 0 | build | M | P0 | 009 | ✓ |
| TI-011 | Explicit source lists | 0 | build | S | P1 | — | ✓ |
| TI-012 | `typeit_legacy` interim library | 0 | refactor | S | P1 | 011 | ✓ |
| TI-013 | `cmake/Sanitizers.cmake` | 0 | build | S | P1 | 010 | |
| TI-014 | `.clang-tidy` and static analysis | 0 | build | M | P2 | 010 | |
| TI-015 | `.editorconfig` and format check | 0 | build | XS | P2 | — | |
| ~~TI-016~~ | *moved →* CI-002, CI-005 | 0 | — | — | — | — | |
| ~~TI-017~~ | *moved →* CI-004 | 0 | — | — | — | — | |
| ~~TI-018~~ | *moved →* CI-007 | 0 | — | — | — | — | |
| ~~TI-019~~ | *moved →* CI-003, CI-005 | 0 | — | — | — | — | |
| TI-020 | Enable `TYPEIT_WERROR` in CI | 0 | ci | XS | P0 | 004,CI-005 | ✓ |
| TI-021 | Documentation relocation, README rewrite | 0 | docs | S | P1 | — | ✓ |
| TI-022 | Contribution scaffolding | 0 | docs | S | P3 | 021 | |
| TI-023 | Scaffold `typeit_core` with isolation | 1 | build | S | P0 | 012 | ✓ |
| TI-024 | `Units.h` strong types | 1 | feat | S | P0 | 023 | ✓ |
| TI-025 | `Result.h` error type | 1 | feat | S | P0 | 023 | ✓ |
| TI-026 | `IClock` and `FakeClock` | 1 | feat | XS | P0 | 024 | ✓ |
| TI-027 | `Grapheme` and UTF-8 decoder | 1 | feat | M | P0 | 025 | ✓ |
| TI-028 | `Segmenter` cluster breaking | 1 | feat | L | P0 | 027 | ✓ |
| TI-029 | Display width tables | 1 | feat | M | P0 | 028 | |
| TI-030 | `TextBuffer` | 1 | feat | M | P0 | 029 | |
| TI-031 | `Wrapper` pure line wrapping | 1 | feat | M | P0 | 030 | ✓ |
| TI-032 | `Keystroke` and `KeystrokeLog` | 1 | feat | S | P0 | 030 | |
| TI-033 | `TypingModel` state machine | 1 | feat | L | P0 | 032 | ✓ |
| TI-034 | `TypingRules` variants | 1 | feat | M | P1 | 033 | |
| TI-035 | `LogBuilder` test DSL | 1 | test | S | P0 | 032 | |
| TI-036 | Metrics: speed | 1 | feat | M | P0 | 035 | ✓ |
| TI-037 | Metrics: accuracy | 1 | feat | M | P0 | 035 | ✓ |
| TI-038 | Metrics: consistency | 1 | feat | S | P1 | 036 | |
| TI-039 | Metrics: rolling and peak WPM | 1 | feat | M | P0 | 036 | |
| TI-040 | Metrics: timeline | 1 | feat | S | P1 | 039 | |
| TI-041 | Per-key and per-bigram statistics | 1 | feat | M | P1 | 035 | |
| TI-042 | Error map | 1 | feat | S | P1 | 041 | |
| TI-043 | `IMode` and `ModeProgress` | 1 | feat | S | P0 | 033 | |
| TI-044 | `TimedMode` | 1 | feat | S | P0 | 043 | ✓ |
| TI-045 | `WordCountMode` | 1 | feat | S | P1 | 043 | |
| TI-046 | `QuoteMode` | 1 | feat | S | P1 | 043 | |
| TI-047 | `ZenMode` | 1 | feat | XS | P2 | 043 | |
| TI-048 | `ITextProvider`, `WholeTextProvider` | 1 | feat | S | P0 | 030 | |
| TI-049 | `ChunkedProvider` | 1 | feat | M | P2 | 048 | |
| TI-050 | `Config` type and validation | 1 | feat | M | P1 | 025 | |
| TI-051 | Port the legacy regression suite | 1 | test | M | P0 | 031,033,037,044 | ✓ |
| TI-052 | Coverage gate for `core` | 1 | test | S | P1 | phase | |
| TI-053 | Scaffold `infra`, add SQLite + toml++ | 2 | build | S | P0 | 023,007 | |
| TI-054 | `PlatformPaths` | 2 | feat | M | P0 | 053 | ✓ |
| TI-055 | `AssetLocator` | 2 | feat | M | P0 | 054 | ✓ |
| TI-056 | `SqliteDatabase` wrapper | 2 | feat | M | P0 | 053 | |
| TI-057 | `Migrator` and schema v1 | 2 | feat | L | P0 | 056 | |
| TI-058 | `SqliteHistoryRepository` | 2 | feat | L | P0 | 057 | |
| TI-059 | `SqliteTextLibraryRepository` | 2 | feat | M | P1 | 057 | |
| TI-060 | `TomlConfigStore` | 2 | feat | M | P0 | 053,050 | ✓ |
| TI-061 | Config key migration | 2 | feat | S | P2 | 060 | |
| TI-062 | `SystemClock`, `StdFileSystem` | 2 | feat | S | P0 | 053 | |
| TI-063 | Contract test harness | 2 | test | M | P0 | 058,059 | |
| TI-064 | Fake adapters | 2 | test | M | P0 | 063 | |
| TI-065 | Hermetic test fixtures | 2 | test | S | P0 | 054 | ✓ |
| TI-066 | Scaffold `app` and ports | 3 | build | S | P0 | 023 | |
| TI-067 | `ConfigService` | 3 | feat | S | P0 | 066,064 | |
| TI-068 | `SessionService` | 3 | feat | L | P0 | 066,064,043 | ✓ |
| TI-069 | `HistoryService` | 3 | feat | M | P1 | 068 | |
| TI-070 | `TextLibraryService` | 3 | feat | M | P1 | 071,072,064 | ✓ |
| TI-071 | `TextNormalizer` | 3 | feat | M | P1 | 030 | |
| TI-072 | Difficulty scoring | 3 | feat | S | P2 | 030 | |
| TI-073 | `ProfileService` | 3 | feat | S | P1 | 069 | |
| TI-074 | CLI argument parser | 3 | feat | M | P1 | 066 | |
| TI-075 | `--simulate` headless harness | 3 | feat | M | P0 | 068,074 | |
| TI-076 | `--doctor` | 3 | feat | S | P2 | 074,055 | |
| TI-077 | `--stats` and `--export` | 3 | feat | S | P2 | 069,074 | |
| TI-078 | Version wiring end to end | 3 | feat | XS | P1 | 002,068,074 | |
| TI-079 | Scaffold `tui` and `TerminalApp` | 4 | build | M | P0 | 066,007 | |
| TI-080 | `FrameTicker` | 4 | feat | S | P0 | 079 | ✓ |
| TI-081 | `ScreenStack` and `IScreen` | 4 | feat | M | P0 | 079 | ✓ |
| TI-082 | `Capabilities::detect()` | 4 | feat | M | P0 | 079 | ✓ |
| TI-083 | `Theme` and `ThemeLoader` | 4 | feat | M | P1 | 082 | |
| TI-084 | `ColorQuantizer` | 4 | feat | M | P1 | 083 | ✓ |
| TI-085 | `GlyphSet` | 4 | feat | S | P1 | 082 | ✓ |
| TI-086 | `Keymap` and binding parser | 4 | feat | M | P1 | 079 | ✓ |
| TI-087 | `TypingArea` widget | 4 | feat | L | P0 | 081,083,085 | ✓ |
| TI-088 | `StatsBar` and `KeyHintBar` | 4 | feat | S | P1 | 083 | |
| TI-089 | Responsive layout and resize | 4 | feat | M | P0 | 087 | ✓ |
| TI-090 | `TerminalTooSmallScreen` | 4 | feat | XS | P1 | 089 | ✓ |
| TI-091 | `MenuScreen` | 4 | feat | M | P0 | 081,086 | ✓ |
| TI-092 | `SessionScreen` | 4 | feat | M | P0 | 087,088,068 | ✓ |
| TI-093 | `ResultsScreen` (parity) | 4 | feat | S | P0 | 092 | |
| TI-094 | `HelpScreen` | 4 | feat | XS | P2 | 086 | |
| TI-095 | Render snapshot harness | 4 | test | M | P0 | 079 | |
| TI-096 | Windows console initialisation | 4 | feat | M | P0 | 079 | ✓ |
| TI-097 | **Cutover: delete the legacy tree** | 4 | refactor | M | P0 | 098 | ✓ |
| TI-098 | Parity verification | 4 | test | M | P0 | 091–096 | |
| TI-099 | `Sparkline` widget | 5 | feat | S | P1 | 095 | |
| TI-100 | `LineChart` widget | 5 | feat | M | P1 | 095 | |
| TI-101 | `Histogram` widget | 5 | feat | S | P2 | 095 | |
| TI-102 | `Heatmap` widget | 5 | feat | M | P1 | 095 | |
| TI-103 | `HistoryScreen` | 5 | feat | L | P1 | 099–102,069 | |
| TI-104 | `SessionDetailScreen` | 5 | feat | S | P2 | 103 | |
| TI-105 | `ResultsScreen` enrichment | 5 | feat | M | P1 | 100,093 | |
| TI-106 | Menu recent-runs sparkline | 5 | feat | XS | P2 | 099,091 | |
| TI-107 | Streaks and daily goals | 5 | feat | S | P2 | 069 | |
| TI-108 | Export from the UI | 5 | feat | XS | P3 | 077,103 | |
| TI-109 | History performance | 5 | perf | M | P1 | 103 | |
| TI-110 | Import from file | 6 | feat | S | P0 | 070 | ✓ |
| TI-111 | Import from stdin | 6 | feat | S | P1 | 110 | |
| TI-112 | Import by paste | 6 | feat | M | P2 | 110 | |
| TI-113 | Content deduplication | 6 | feat | S | P1 | 110 | |
| TI-114 | Tagging and search | 6 | feat | S | P2 | 059 | |
| TI-115 | Bookmarks and chunk progress | 6 | feat | M | P1 | 049,059 | |
| TI-116 | `ShuffledSentenceProvider` | 6 | feat | M | P1 | 048 | |
| TI-117 | `WordPoolProvider` | 6 | feat | M | P0 | 048 | |
| TI-118 | `TextLibraryScreen` | 6 | feat | M | P1 | 110–115 | |
| TI-119 | Text CLI flags | 6 | feat | S | P2 | 074,110 | |
| TI-120 | `EndlessMode` | 7 | feat | S | P1 | 043,117 | |
| TI-121 | `Pacer` | 7 | feat | M | P0 | 043 | |
| TI-122 | `DifficultyController` ramp law | 7 | feat | L | P0 | 121 | |
| TI-123 | `RaceParams` and presets | 7 | feat | S | P1 | 122 | |
| TI-124 | `RaceMode` | 7 | feat | L | P0 | 122,123 | |
| TI-125 | Race HUD | 7 | feat | M | P1 | 124,087 | |
| TI-126 | Adaptive start speed | 7 | feat | S | P0 | 073,124 | |
| TI-127 | Race persistence | 7 | feat | S | P1 | 124,058 | |
| TI-128 | Speed-wall analysis | 7 | feat | M | P1 | 127,105 | |
| TI-129 | Ramp tuning pass | 7 | chore | M | P1 | 124–128 | |
| TI-130 | Drill target selection | 8 | feat | M | P1 | 041,042,058 | |
| TI-131 | Drill text synthesis | 8 | feat | M | P1 | 130 | |
| TI-132 | `DrillMode` and screen | 8 | feat | M | P1 | 131,043 | |
| TI-133 | Race-to-drill handoff | 8 | feat | S | P2 | 128,132 | |
| TI-134 | Daily goal and streak UI | 8 | feat | S | P3 | 107 | |
| TI-135 | Terminal compatibility pass | 9 | test | L | P0 | phase 8 | ✓ |
| TI-136 | `--doctor` key tester | 9 | feat | S | P2 | 076,135 | ✓ |
| TI-137 | Install rules | 9 | build | S | P0 | 055 | ✓ |
| TI-138 | CPack packaging | 9 | build | M | P1 | 137 | |
| TI-139 | Man page and `--help` | 9 | docs | S | P2 | 074 | |
| TI-140 | Release CI | 9 | ci | M | P1 | 138 | |
| TI-141 | Fuzz targets | 9 | test | M | P2 | phase 6 | |
| TI-142 | Endless-mode soak test | 9 | test | S | P2 | 120 | |
| TI-143 | README and screenshots | 9 | docs | M | P1 | 135 | |
| TI-144 | Documentation reconciliation | 9 | docs | M | P0 | all | ✓ |
| TI-145 | 2.0.0 release | 9 | chore | S | P0 | 135–144 | |
| TX-001 | Split ingestion into fetch and extract | 6A | refactor | M | P0 | TI-110,TI-118 | ✓ |
| TX-002 | Markdown extractor | 6A | feat | S | P1 | TX-001 | |
| TX-003 | Code extractor | 6A | feat | M | P1 | TX-001 | |
| TX-004 | Subtitle extractor, directory import | 6A | feat | S | P2 | TX-001 | |
| TX-005 | Sections model | 6A | feat | M | P1 | TX-002,TX-003 | |
| TX-006 | Schema v2 and section persistence | 6A | feat | M | P1 | TX-005,TI-057 | |
| TX-007 | Typing-readiness pass | 6A | feat | L | P1 | TX-005 | ✓ |
| TX-008 | EPUB extractor | 6A | feat | L | P1 | TX-006,TX-007 | |
| TX-009 | External converter hook | 6A | feat | M | P1 | TX-001 | |
| TX-010 | HTTP fetcher | 6A | feat | L | P2 | TX-001 | |
| TX-011 | HTML readability extractor | 6A | feat | L | P2 | TX-010 | |
| TX-012 | Web import UX and CLI | 6A | feat | M | P2 | TX-011,TI-118 | |

**Totals** — 171 active issues.

| Priority | | Size | | Type | |
|---|---|---|---|---|---|
| P0 | 74 | XS | 13 | feat | 109 |
| P1 | 62 | S | 64 | ci | 20 |
| P2 | 30 | M | 77 | build | 16 |
| P3 | 5 | L | 17 | test | 11 |
| | | | | docs | 5 |
| | | | | fix / refactor / chore | 3 / 3 / 3 |
| | | | | perf | 1 |

P0 at 43% is high, and that is a real observation rather than a scheduling accident: phases 0A
through 4 are almost entirely load-bearing, because a pipeline and a migration both have few
optional steps until they are done. From Phase 5 onward the mix shifts sharply toward P1/P2,
which is where the schedule actually has slack. **If time gets short, cut from phases 5, 6, 6A,
and 8 — not from 0A through 4.**

The `ci` count (20) is second only to `feat`, which is the intended shape: the pipeline is a
deliverable in its own right, not an afterthought bolted on at the end.

---

## Pushing these to GitHub

The backlog lives in this repository as the source of truth. To mirror it as GitHub issues
(`gh` is not currently authenticated here — run `gh auth login` first, and note that creating
issues is a public, outward-facing action on `KriZa96/TypeIt`):

1. Create the ten milestones from the table at the top.
2. Create the labels listed above.
3. Create one issue per `##` heading, using its body verbatim; put the `TI-###` id in the
   title so the cross-references in these documents keep resolving.
4. Link dependencies with GitHub's issue references after the ids are assigned.

Keeping the markdown as the source of truth means the backlog survives outside GitHub and
reviews as part of a normal diff. If they diverge, these files win.
