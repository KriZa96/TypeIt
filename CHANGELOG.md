# Changelog

All notable changes to this project are documented here.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and this project
adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html) as narrowed by
[docs/VERSIONING.md](docs/VERSIONING.md).

## [Unreleased]

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
  (TI-029). None of it is wired into the application yet; the 1.0 tree is untouched.

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

### Fixed

- The Windows builds no longer pass `-DCMAKE_CXX_FLAGS=-utf-8`, which replaced CMake's default
  `/EHsc` and broke both MSVC and clang-cl on the first Windows run this project has ever had.
  `/utf-8` was already applied by `cmake/CompilerWarnings.cmake` (CI-004).
- Opening an existing but empty file no longer calls `pop_back` on an empty string, which was
  undefined behaviour (TI-005).
- The no-op `ExitLoopClosure` call in the menu is removed; quitting already went through the
  screen's own exit path (TI-006).

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
