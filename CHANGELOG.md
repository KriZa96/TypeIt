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

### Changed

- `TimerTest.DoesNotCalculateWhenStartGameFalse` is renamed from its misspelled form, and the
  misspelled fixture text in the line engine tests is corrected (CI-006).
- The whole tree is formatted to the repository's `.clang-format` for the first time (CI-006).
- `CMakeLists.txt` no longer overrides a compiler or toolchain chosen on the command line or in
  the environment (CI-005; TI-009 removes the assignment entirely).

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
