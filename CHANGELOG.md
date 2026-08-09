# Changelog

All notable changes to this project are documented here.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and this project
adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html) as narrowed by
[docs/VERSIONING.md](docs/VERSIONING.md).

## [Unreleased]

### Changed — breaking

These are the three places 2.0 does not agree with 1.0. Everything else is additive.

- **Metric definitions.** Words-per-minute is now divided by the span of the run's own
  keystroke log — first keystroke to last — rather than by the duration the mode was configured
  with, so a thirty-second run abandoned after five seconds no longer reports a sixth of the
  speed it was typed at (defect C5). Accuracy is first-attempt accuracy and is no longer
  reduced by pressing backspace; what is still wrong at the end is reported separately as
  *correctness* (defect C4). A history exported from 1.0 and one exported from 2.0 are not
  comparable figure for figure.
- **Keybindings.** `Ctrl+T` is retired: it is SIGINFO on BSD, a tab key in several emulators,
  and the common tmux prefix. Quitting is `Ctrl+Q`, going back is `Escape`, restarting is
  `Ctrl+R`, and help is `F1`. Every one of them is rebindable from `config.toml`, which 1.0 had
  no way to do.
- **Data locations.** Settings, history and assets live in the platform's directories
  (`$XDG_CONFIG_HOME/typeit`, `$XDG_DATA_HOME/typeit`, and the asset search path of
  TECHNICAL §4.1) instead of beside the source tree. 1.0 located its bundled texts with
  `__FILE__` at runtime, so the binary only worked on the machine that compiled it, with the
  sources still in place (defect C2). The bundled corpora have moved from `files/` to
  `assets/texts/` to match.

### Added

- Continuous integration: build and test matrix across Linux gcc, Linux clang and Windows MSVC
  and clang-cl, with a non-blocking macOS job (CI-002, CI-004, CI-005).
- A shared `setup-build` composite action with dependency and compiler caching (CI-003).
- Quality gates: clang-format, clang-tidy, cppcheck, typos, markdownlint, link checking,
  shellcheck, workflow lint, and Conventional Commits on pull request titles (CI-001, CI-006).
- Sanitizer job running the suite under ASan and UBSan with hardened-mode assertions (CI-007).
- `CMakePresets.json` with the gcc, clang, ASan/UBSan, clang-tidy, coverage and vcpkg presets
  used by both CI and local builds (TI-010).
- `cmake/CompilerWarnings.cmake` with the GCC/Clang and MSVC warning sets and the
  `TYPEIT_WERROR` option, now on in every CI job (TI-003, TI-020).
- `cmake/Sanitizers.cmake` and `cmake/StaticAnalysis.cmake`, plus per-directory `.clang-tidy`
  overrides so the legacy tree can be checked without blocking on its known findings
  (TI-013, TI-014).
- A generated `typeit/core/Version.h`: the version is declared once in `CMakeLists.txt` and
  read from the header everywhere else (TI-002).
- `.editorconfig` (TI-015) and `CONTRIBUTING.md` with GitHub issue and pull request templates
  (TI-022).
- Version guards: the tag, the source, the pre-release suffix and the CHANGELOG heading can no
  longer disagree, and no version literal may appear outside `CMakeLists.txt`. Each guard has a
  fixture that watches it fail (CI-008).
- A dispatchable version bump that edits the version, moves the `Unreleased` block under a
  dated heading, and opens a pull request rather than pushing (CI-012).
- Security and supply chain: CodeQL, MSVC `/analyze`, dependency review, OSV, OpenSSF Scorecard,
  and Dependabot for GitHub Actions versions (CI-010).
- A nightly workflow building the documented compiler floor exactly, plus aarch64, Alpine/musl,
  shared libraries, ThreadSanitizer, Valgrind and a dependency freshness report (CI-013).
- A documentation site: mdBook over `docs/` and Doxygen over the public headers, published to
  GitHub Pages from `main` (CI-016).
- Repository automation: path-based labelling, `CODEOWNERS`, Release Drafter, and the labels and
  branch protection scripted in `scripts/repo-settings.sh` (CI-018).
- `typeit::core`, the domain library, built with nothing on its link line so a dependency on the
  terminal is a compile error rather than a review comment (TI-023). It carries the strong
  domain types (TI-024), the `Result` error type (TI-025), the `IClock` time port (TI-026), and
  the text foundation: a UTF-8 decoder that reports the byte it rejected (TI-027), grapheme
  cluster segmentation (TI-028), and display width tables generated from Unicode 17.0.0
  (TI-029).
- **Text is grapheme clusters, not bytes.** `TextBuffer` indexes in clusters, so a two-byte `č`
  is one character to type rather than two, and line wrapping is a pure function of text and
  column count rather than state on the buffer — which is what makes a terminal resize a matter
  of calling it again (TI-030, TI-031).
- **The keystroke log is the single source of truth.** Every metric is a pure function over an
  append-only log of what was pressed and when (TI-032, TI-033, TI-035), which is what makes the
  1.0 accuracy defect unrepresentable rather than merely fixed. Typing rules — whether an error
  stops you, whether a missing space is an error — are variants over the same model (TI-034).
- Metrics: raw, gross and net speed (TI-036); first-attempt accuracy and end-state correctness
  as separate figures (TI-037); consistency (TI-038); rolling and peak-sustained WPM (TI-039);
  a per-second timeline (TI-040); per-key and per-bigram statistics (TI-041); and the
  expected-to-typed error map that drills are later built from (TI-042).
- Modes behind one interface, so adding one needs no migration: timed, word count, quote and zen
  (TI-043 – TI-047). Text supply is a separate interface again — whole text, chunked, shuffled
  sentences and a word pool — each driven by a seed that reproduces the exact stream
  (TI-048, TI-049, TI-116, TI-117).
- A configuration value type with pure validation, so a bad setting is a message rather than a
  crash or a silent default (TI-050).
- `typeit::infra`, the driven adapters: a SQLite connection with a statement cache and
  transactions (TI-056), the migrator and schema v1 (TI-057), the history and text-library
  repositories (TI-058, TI-059), the TOML configuration store with key migration
  (TI-060, TI-061), platform paths and asset location that no longer depend on `__FILE__`
  (TI-054, TI-055), and the system clock and filesystem (TI-062).
- `typeit::app`, the use cases: config, session, history, text library and profile services over
  declared ports (TI-066 – TI-070, TI-073), with import-time normalisation and difficulty
  scoring in `core` where they belong (TI-071, TI-072). Every port has a contract suite run
  against both the real adapter and a fake, so the two cannot disagree (TI-063, TI-064).
- A command line: argument parsing (TI-074), `--simulate` for running a whole session headless
  from a keystroke script (TI-075), `--doctor` for terminal capabilities, paths and database
  health (TI-076), and `--stats` / `--export` in CSV and JSON (TI-077, TI-078).
- `typeit::tui`, the terminal frontend: a frame ticker, a screen stack, capability detection,
  theming with colour quantisation for terminals that cannot do true colour, glyph sets for
  terminals without Unicode, and a rebindable keymap (TI-079 – TI-086). Then the widgets and
  screens themselves — typing area, stats and key-hint bars, responsive layout, and the menu,
  session, results, help and too-small screens (TI-087 – TI-096), with a render snapshot harness
  and the assertion that `Render()` mutates nothing (TI-095).
- History: sparkline, line chart, histogram and heatmap widgets (TI-099 – TI-102); a history
  screen and a per-run detail screen (TI-103, TI-104); an enriched results screen (TI-105); the
  menu's recent-runs sparkline (TI-106); streaks counted against the daily goal rather than
  against turning up (TI-107); and export from the interface (TI-108). The daily totals are
  aggregated by SQL, so the memory a history screen needs stops growing with how much somebody
  has typed (TI-109).
- A text library. Import from a file, from standard input or by paste (TI-110 – TI-112);
  deduplication by content hash, so the same file imported twice is one text (TI-113); tags and
  search (TI-114); bookmarks that advance by what was actually typed rather than by the chunk
  that was offered (TI-115); a library screen (TI-118); and the flags to drive all of it from a
  command line (TI-119).
- **Import is a pipeline, and formats plug into it.** Acquisition, extraction and normalisation
  are separate stages resolved by MIME type, so adding a format is one new file and one
  registration (TX-001). Markdown extracts with its markup stripped and its code fences intact
  (TX-002); source code keeps its indentation byte for byte and warns when a file mixes tabs and
  spaces (TX-003); SubRip and WebVTT subtitles come back as re-joined sentences rather than as
  the subtitler's line wrapping (TX-004). A whole folder imports at once, reporting what it
  skipped instead of failing on the first bad file (TX-004).
- **Sections.** A chapter has a name and a grapheme range, and the ranges cover a text end to
  end with no gaps, so a bookmark always lands in exactly one of them (TX-005). They are
  persisted, along with which section a bookmark is in and what read the text in the first place
  (TX-006).
- **A typing-readiness pass**, which is the difference between a feature and a trap. An
  extracted chapter arrives with running heads, page numbers, footnote markers, words broken
  across line breaks and typography no keyboard produces; the pass removes what it recognises
  and *reports* what it cannot, counting every character a standard keyboard cannot reach and
  saying for each whether normalisation will rescue it (TX-007). `inspect_file` produces that
  report before anything is stored, so the answer to "is this worth typing" arrives before the
  library grows.

### Changed

- Dependencies are fetched with `FetchContent` from `cmake/Dependencies.cmake` instead of the
  vcpkg submodule, so a fresh clone needs no submodule step (TI-007, TI-008).
- Sources are listed explicitly instead of globbed, and the legacy code is built as the interim
  `typeit_legacy` library that the tests link against (TI-011, TI-012).
- Every warning the new flag set surfaced is fixed — the `-Wreorder`, `-Wparentheses` and
  signed/unsigned findings — with behaviour pinned by the existing suite (TI-004).
- Documentation lives in one place: `src/docs/` is deleted and `README.md` is a README again,
  linking into `docs/` (TI-021).
- `TimerTest.DoesNotCalculateWhenStartGameFalse` is renamed from its misspelled form, and the
  misspelled fixture text in the line engine tests is corrected (CI-006).
- The whole tree is formatted to the repository's `.clang-format` for the first time (CI-006).
- `CMakeLists.txt` no longer overrides a compiler or toolchain chosen on the command line or in
  the environment (CI-005, TI-009).
- The database schema is at `user_version` 3. Version 2 added a covering index for the history
  screen's daily aggregation (TI-109); version 3 added the `text_section` table, `section_idx`
  on bookmarks, and `author` / `mime` / `extractor` on `text_item` (TX-006). Both migrate
  in place: an existing library keeps every text and every bookmark, and each text gains the
  single section it always implicitly had.
- Line endings are LF in the working tree on every platform (`.gitattributes`). Two things
  depended on it without saying so: the rendered-screen fixtures are compared byte for byte, and
  the schema files are read at build time and embedded in the binary.

### Fixed

- The Windows builds no longer pass `-DCMAKE_CXX_FLAGS=-utf-8`, which replaced CMake's default
  `/EHsc` and broke both MSVC and clang-cl on the first Windows run this project has ever had.
  `/utf-8` was already applied by `cmake/CompilerWarnings.cmake` (CI-004).
- Opening an existing but empty file no longer calls `pop_back` on an empty string, which was
  undefined behaviour (TI-005).
- The no-op `ExitLoopClosure` call in the menu is removed; quitting already went through the
  screen's own exit path (TI-006).
- The history screen's daily totals no longer fail on any build that does not find a system
  SQLite. `floor()` has been opt-in since SQLite 3.35 and the bundled amalgamation was built
  without it, so the query failed with `no such function: FLOOR` — on Windows always, and on
  Linux never, which is why it went unnoticed until the Windows job first got far enough to run
  its tests.
- `--import-dir` imports a directory instead of reporting that the feature is not built. The
  capability landed in TX-004 without the flag being connected to it.
- `typeit --import` prints what the extractor and the readiness pass found — a file that mixes
  tabs and spaces, characters no keyboard can reach — instead of computing both and discarding
  them.

### Known gaps

Recorded here because they are visible from the outside, not to excuse them.

- **The 1.0 tree is still present and still built.** The rebuild is complete and is what
  `typeit` runs; `TypeIt` is the 1.0 binary, kept alongside it. Deleting it is TI-097, which is
  gated on a manual four-terminal compatibility sweep in TI-098 that nobody has done yet.
- Endless mode, race mode and drills are declared by `--help` and are not implemented
  (phases 7 and 8). `--url` likewise: web import is wave 4 of Phase 6A and ships in 2.1.0.
- `--section` is parsed and not yet acted on.

## [1.0.0] - 2025-05-17

The original FTXUI typing test, tagged retroactively at commit `d7a2d1c` with no code change,
so that the starting point of the 2.0 rebuild is a named, checkoutable thing.

### Added

- Three bundled difficulties — simple, medium, hard — plus typing from a custom file path.
- Timed runs of 15, 30 or 60 seconds, or a custom duration.
- Live words per minute and accuracy while typing.
- Per-character colour feedback, restart, and exit to menu.

### Notes

- **1.0.0 persists nothing.** No configuration, no history, no database, no files written
  outside the build tree. There is therefore no data migration into 2.0.0 and none is needed.

[Unreleased]: https://github.com/KriZa96/TypeIt/compare/v1.0.0...HEAD
[1.0.0]: https://github.com/KriZa96/TypeIt/releases/tag/v1.0.0
