# Phase 0 — Foundation

**Milestone:** `v2.0.0-alpha.1` · **Issues:** TI-001 – TI-022 · **Goal:** make the build tell
the truth. No behaviour changes.

> **Runs after [Phase 0A](PHASE-0A-cicd.md).** The pipeline is built first, against the
> codebase as it stands, so this phase's changes are guarded from the first commit. TI-016
> through TI-020 have moved into Phase 0A — see the note at each.

The existing application must keep building and passing its 41 tests at every commit in this
phase. Nothing here changes what the program does; everything here changes what the build is
able to tell you about it.

---

## TI-001 — Tag 1.0.0 and create CHANGELOG.md

**Type** chore · **Size** XS · **Priority** P0 · **Depends on** — · **Docs** [VERSIONING §2](../VERSIONING.md#2-the-two-versions-that-exist)

Name the starting point before changing anything, so "how it behaved before" stays
checkoutable.

**Scope**
- In: annotated tag `v1.0.0` on `d7a2d1c`; `CHANGELOG.md` in Keep a Changelog format with a
  `1.0.0` entry describing today's capabilities and an empty `Unreleased` block; create the
  `v2` integration branch.
- Out: any code change.

**Tests** — none (no code).

**Acceptance**
- [x] `git show v1.0.0` resolves to `d7a2d1c`.
- [x] `CHANGELOG.md` records 1.0.0 capabilities and notes that it persists no user data.
- [x] Branch `v2` exists and is where all Phase 0–3 work lands.

---

## TI-002 — Single-source version and generated `Version.h`

**Type** build · **Size** S · **Priority** P1 · **Depends on** TI-001 · **Docs** [VERSIONING §6](../VERSIONING.md#6-single-source-of-truth)

Declare the version once and generate a header from it. Every later consumer — `--version`,
`session.app_version`, packaging — reads that header.

**Scope**
- In: `project(TypeIt VERSION 2.0.0)`; `TYPEIT_VERSION_PRERELEASE` cache variable;
  `configure_file` producing `typeit/core/Version.h`; git-describe capture at configure time.
- Out: `--version` flag (needs the CLI — TI-078).

**Unit tests** (`VersionTest.cpp`)
- `kVersionString` matches `MAJOR.MINOR.PATCH[-prerelease]` exactly.
- `kVersionMajor/Minor/Patch` match the parsed components of `kVersionString`.
- An empty `TYPEIT_VERSION_PRERELEASE` yields no trailing hyphen.

**Acceptance**
- [x] No version literal exists anywhere outside `CMakeLists.txt` (`git grep` clean, and
      enforced by the CI-008 guard).
- [x] Generated header lands in the build tree, not the source tree.
- [x] Configuring in a tarball with no `.git` succeeds; `kGitDescribe` is empty.

---

## TI-003 — `cmake/CompilerWarnings.cmake`

**Type** build · **Size** S · **Priority** P0 · **Depends on** — · **Docs** [BUILD §6](../BUILD.md#6-warnings)

Add the warning set from BUILD.md as a reusable `typeit_set_warnings(target)` function and
apply it to the existing targets. **`-Werror` stays off** until TI-004 clears the baseline.

**Scope**
- In: GCC/Clang and MSVC warning sets; `/utf-8` on MSVC; `TYPEIT_WERROR` option, default OFF.
- Out: fixing any warning (TI-004).

**Tests** — build-level. Capture the baseline warning output as `docs/warning-baseline.txt` so
TI-004 can be verified against it, and delete that file when TI-004 closes.

**Acceptance**
- [x] Warnings appear on gcc and clang builds.
- [x] Build still succeeds (warnings are not yet errors).
- [x] Baseline captured and committed.

---

## TI-004 — Clear the warning baseline to zero

**Type** fix · **Size** M · **Priority** P0 · **Depends on** TI-003 · **Docs** [REVIEW C6–C8](../CODEBASE_REVIEW.md#4-correctness-defects)

Fix every warning the new flags surface. Known in advance:

| Warning | Where |
|---|---|
| `-Wreorder` ×3 | `Input`, `Menu`, `Timer` constructor initialiser lists |
| `-Wparentheses` | `Text.cpp:47` — `&&`/`\|\|` mixed without parentheses |
| `-Wsign-compare` / `-Wsign-conversion` | `InputLineEngine` throughout; `Text::populate_text_lines` `int index` vs `text_.size()` |

**Scope**
- In: reorder initialiser lists to match declaration order; parenthesise the wrap condition
  **preserving current behaviour exactly**; fix signed/unsigned comparisons with explicit casts
  or type changes.
- Out: redesigning any of these classes — they are deleted in Phase 4. Minimal, behaviour-
  preserving fixes only.

**Unit tests**
- All 41 existing tests still pass unchanged. **No test may be modified in this issue** — if a
  test fails, the fix changed behaviour and is wrong.
- Add `TextTest.WrapConditionUnchangedAfterParenthesisation`: assert line counts for the same
  inputs as the existing wrapping tests, pinning behaviour across the edit.

**Acceptance**
- [x] Zero warnings on gcc and clang with the TI-003 flag set.
- [x] Zero warnings on MSVC `/W4`, now that a Windows job gets far enough to prove it.
- [x] `docs/warning-baseline.txt` deleted.
- [ ] Existing test suite passes with no test file modified.

> Two test files were modified after all: `int` counters passed to a `std::size_t` parameter,
> and one `EXPECT_FLOAT_EQ` against an `int`-returning function. The warning set applies to the
> test target too, so zero warnings is unreachable otherwise. No expectation changed — see
> commit `e0db7f3`.

---

## TI-005 — Fix C1: `pop_back` on empty content in `FileTextSource::get_text`

**Type** fix · **Size** XS · **Priority** P1 · **Depends on** — · **Docs** [REVIEW C1](../CODEBASE_REVIEW.md#4-correctness-defects)

An existing-but-empty file opens successfully, the read loop never runs, and
`content.pop_back()` on an empty `std::string` is undefined behaviour. Currently masked by
`is_file_valid()` upstream, but `get_text()` is public and independently tested.

**Scope**
- In: guard the `pop_back`; return an empty string for an empty file.
- Out: changing `is_file_valid` semantics.

**Unit tests** (`test_file_text_source.cpp`)
- `ReadsEmptyFileReturnsEmptyString` — a zero-byte file returns `""` and does not crash.
- `ReadsWhitespaceOnlyFile` — a file containing only `"   "` returns it without underflow.
- `ReadsSingleCharacterFile` — boundary at exactly one character.
- The new tests must fail on the unfixed code. Verify that before fixing.

**Acceptance**
- [x] New tests fail before the fix, pass after — verified by reverting `FileTextSource.cpp`
      alone: `ReadsEmptyFileReturnsEmptyString` aborts.
- [x] Clean under ASan/UBSan.

---

## TI-006 — Fix C3: no-op `ExitLoopClosure` in `Menu::exit_application`

**Type** fix · **Size** XS · **Priority** P3 · **Depends on** — · **Docs** [REVIEW C3](../CODEBASE_REVIEW.md#4-correctness-defects)

`screen_.ExitLoopClosure();` constructs a closure and discards it. Only the following
`screen_.Exit()` does anything. Delete the dead statement.

**Scope** — one line.

**Unit tests** — none practical (requires a live screen). Covered by manual verification;
proper coverage arrives with `MenuScreen` in Phase 4.

**Acceptance**
- [ ] Statement removed; exit still works when run manually.

---

## TI-007 — `cmake/Dependencies.cmake` with FetchContent + find_package fallback

**Type** build · **Size** M · **Priority** P0 · **Depends on** — · **Docs** [BUILD §4](../BUILD.md#4-dependencies-adr-007), [ADR-007](../ARCHITECTURE.md#adr-007--dependencies-via-fetchcontent-with-find_package-fallback-no-vendored-vcpkg)

Central dependency declaration using `FetchContent_Declare(... FIND_PACKAGE_ARGS ...)` so a
system package is preferred and a download is the fallback.

**Scope**
- In: FTXUI and GoogleTest (the only current dependencies), tag-pinned; `TYPEIT_BUILD_TESTS`
  gates GoogleTest; `gtest_force_shared_crt` for Windows.
- Out: SQLite and toml++ (added in Phase 2 when first needed).

**Tests** — build-level, all four paths verified:
1. distro packages present → nothing downloaded
2. no packages → FetchContent downloads and builds
3. vcpkg toolchain → `find_package` resolves via vcpkg
4. `FETCHCONTENT_FULLY_DISCONNECTED=ON` with pre-populated sources

**Acceptance**
- [ ] All four paths configure and build.
- [x] Every git dependency is tag-pinned; no floating refs.
- [x] CMake minimum raised to 3.24 (`FIND_PACKAGE_ARGS`).

> Paths 1, 2 and 4 are verified: an installed GoogleTest resolves through `find_package` while
> FTXUI downloads, in the same configure, and `FETCHCONTENT_FULLY_DISCONNECTED=ON` builds
> against a pre-populated `_deps`. Path 3 needs a vcpkg toolchain, which no longer ships in the
> repository — it is checked by hand or through the `vcpkg` preset.

---

## TI-008 — Remove the vcpkg submodule

**Type** build · **Size** S · **Priority** P0 · **Depends on** TI-007 · **Docs** [REVIEW D7](../CODEBASE_REVIEW.md#5-build-tooling-and-ci)

A ~600 MB submodule for two dependencies, currently uninitialised in the working tree, so the
project does not configure from a fresh clone without `--recursive`.

**Scope**
- In: `git rm` the submodule, delete `.gitmodules`, remove the `external/` directory, drop the
  submodule step from CI and the README.
- Out: deleting `vcpkg.json` / `vcpkg-configuration.json` — both are **kept** so vcpkg stays a
  supported opt-in.

**Tests** — clone to a clean directory *without* `--recursive` and confirm configure + build +
test succeeds. This is the acceptance test.

**Acceptance**
- [x] `git clone <url> && cmake --preset … && cmake --build … && ctest …` works with no
      submodule step.
- [ ] `vcpkg.json` still present and still usable via the vcpkg toolchain.
- [x] No `.gitmodules`, no `external/`.

---

## TI-009 — Remove hardcoded compilers and toolchain assignment

**Type** build · **Size** XS · **Priority** P0 · **Depends on** TI-007 · **Docs** [REVIEW D1, D2](../CODEBASE_REVIEW.md#5-build-tooling-and-ci)

The top-level `CMakeLists.txt` sets `CMAKE_C_COMPILER`/`CMAKE_CXX_COMPILER` and
`CMAKE_TOOLCHAIN_FILE` as normal variables. **Partly addressed by CI-005**, which had to make
both conditional — otherwise the `linux-clang-debug` and `windows-clang-cl` jobs would have
silently built with gcc and cl. They are still assigned, just no longer unconditionally, and
this issue deletes the block outright.

**Scope**
- In: delete both `set()` blocks; compiler comes from the preset or `CC`/`CXX`; toolchain comes
  from a preset.
- Out: writing the presets (TI-010).

**Acceptance**
- [x] Configures with `CXX=g++`, `CXX=clang++`, and MSVC — one CI job apiece, plus clang-cl.
- [ ] `-DCMAKE_TOOLCHAIN_FILE=…` on the command line is honoured.

---

## TI-010 — `CMakePresets.json`

**Type** build · **Size** M · **Priority** P0 · **Depends on** TI-009 · **Docs** [BUILD §7](../BUILD.md#7-presets)

Configure, build, and test presets for every supported toolchain.

**Scope**
- In: `linux-gcc-debug`, `linux-gcc-release`, `linux-clang-debug`, `linux-clang-asan`,
  `linux-tidy`, `linux-coverage`, `windows-msvc-debug`, `windows-msvc-release`,
  `windows-clang-cl`, `vcpkg`. `CMAKE_EXPORT_COMPILE_COMMANDS=ON` on every Ninja preset.
- Out: sanitizer and tidy implementation (TI-013, TI-014) — presets may reference options that
  land in those issues.

**Acceptance**
- [x] `cmake --preset linux-gcc-debug && cmake --build --preset … && ctest --preset …` works.
- [ ] The equivalent Windows sequence works. **Unverified, and CI does not verify it:** the
      jobs configure with `cmake -S . -B build` and named cache variables, not with
      `--preset` — their *names* match the presets but nothing invokes one. So
      [BUILD §9](../BUILD.md#9-continuous-integration)'s "every preset in §7 is invoked by a
      job, so a broken preset is caught immediately" is not true today, and the three Windows
      presets in particular have never been run by anything.
- [x] `compile_commands.json` is generated and clangd resolves includes.

---

## TI-011 — Replace `file(GLOB)` with explicit source lists

**Type** build · **Size** S · **Priority** P1 · **Depends on** — · **Docs** [REVIEW D4, D6](../CODEBASE_REVIEW.md#5-build-tooling-and-ci)

Globbed sources do not trigger reconfigure, and the glob in `tests/` plus an explicit entry
means `test_input_line_engine.cpp` is listed twice.

**Scope**
- In: explicit lists in `src/CMakeLists.txt` and `tests/CMakeLists.txt`; remove the duplicate.
- Out: target restructuring (TI-012).

**Acceptance**
- [x] No `file(GLOB)` remains.
- [x] Each test source appears exactly once.
- [x] Adding a new file to a list and rebuilding picks it up without a manual reconfigure.

---

## TI-012 — Extract `typeit_legacy` interim library

**Type** refactor · **Size** S · **Priority** P1 · **Depends on** TI-011 · **Docs** [REVIEW D5](../CODEBASE_REVIEW.md#5-build-tooling-and-ci)

The test target currently recompiles every application source, so everything builds twice.
Extract the current sources into one static library that both the executable and the tests
link. This target is deleted at the Phase 4 cutover (TI-097); it exists only to stop paying the
double-build cost for the duration of the migration.

**Scope**
- In: `typeit_legacy` static library; `TypeIt` executable links it; tests link it; warnings
  applied to it.
- Out: any change to the code inside it.

**Acceptance**
- [x] Each source compiles exactly once per configuration.
- [x] All existing tests pass unchanged.
- [ ] Incremental build time after touching one `.cpp` measurably drops.

---

## TI-013 — `cmake/Sanitizers.cmake`

**Type** build · **Size** S · **Priority** P1 · **Depends on** TI-010 · **Docs** [BUILD §5](../BUILD.md#5-options)

`typeit_enable_sanitizers(target)` wiring ASan and UBSan behind `TYPEIT_SANITIZERS`, used by the
`linux-clang-asan` preset.

**Scope**
- In: `address` and `undefined`, `-fno-omit-frame-pointer`, `-fno-sanitize-recover=all`; a
  suppressions file if FTXUI or GoogleTest need one.
- Out: TSan and MSan (no threading model worth checking until Phase 4, and one thread that
  posts events is not it).

**Tests** — the existing suite must pass clean under ASan+UBSan. Expect TI-005's empty-file
case to be caught here if TI-005 has not landed; that is a useful cross-check.

**Acceptance**
- [x] `ctest --preset linux-clang-asan` passes with no findings.

---

## TI-014 — `.clang-tidy` and `cmake/StaticAnalysis.cmake`

**Type** build · **Size** M · **Priority** P2 · **Depends on** TI-010 · **Docs** [STYLE §12](../STYLE.md#12-what-ci-enforces)

**Scope**
- In: `.clang-tidy` enabling `bugprone-*`, `cppcoreguidelines-*`, `performance-*`,
  `readability-*`, `modernize-*`, with a documented exclusion list; `TYPEIT_CLANG_TIDY` option;
  optional cppcheck and IWYU targets.
- Out: fixing every finding in the legacy code — it is deleted in Phase 4. Legacy sources are
  excluded via a `.clang-tidy` override; **new code is clean from day one**.

**Acceptance**
- [x] `cmake --preset linux-tidy && cmake --build …` runs tidy.
- [x] Legacy exclusion is explicit, commented, and dated with the issue that removes it.

---

## TI-015 — `.editorconfig` and clang-format CI check

**Type** build · **Size** XS · **Priority** P2 · **Depends on** — · **Docs** [STYLE §1](../STYLE.md#1-language-and-formatting)

**Scope**
- In: `.editorconfig` (UTF-8, LF, final newline, no trailing whitespace); a CI step running
  `clang-format --dry-run --Werror`; keep the existing `.clang-format` unchanged.
- Out: reformatting the tree — do that as one separate, isolated commit so it never pollutes a
  behavioural diff.

**Acceptance**
- [ ] CI fails on a deliberately misformatted file.
- [x] The tree is format-clean — with clang-format 18.1.8 specifically, which is what CI
      pins and what a newer local binary will disagree with.

---

## TI-016 – TI-020 — Moved to Phase 0A

**Status:** superseded. CI is now built *before* this phase rather than partway through it, so
these five issues live in [PHASE-0A-cicd.md](PHASE-0A-cicd.md). The ids are retained here so
existing cross-references keep resolving.

| Was | Now | Note |
|---|---|---|
| TI-016 — CI: Linux matrix | [CI-002](PHASE-0A-cicd.md#ci-002--build-and-test-the-current-code-on-linux), [CI-005](PHASE-0A-cicd.md#ci-005--full-matrix-and-the-aggregator-check) | Split: bootstrap job first, full matrix once presets exist |
| TI-017 — CI: Windows MSVC | [CI-004](PHASE-0A-cicd.md#ci-004--windows-job) | Moved much earlier — its first failure is the most valuable output of the whole phase |
| TI-018 — CI: sanitizers | [CI-007](PHASE-0A-cicd.md#ci-007--sanitizer-workflow) | Now its own workflow, with hardened-mode assertions |
| TI-019 — CI: clang-cl + caching | [CI-003](PHASE-0A-cicd.md#ci-003--composite-setup-action-and-caching), [CI-005](PHASE-0A-cicd.md#ci-005--full-matrix-and-the-aggregator-check) | Caching promoted into the shared composite action |
| TI-020 — Enable `TYPEIT_WERROR` | Stays here, below | It depends on TI-004 clearing the baseline, which is Phase 0 work |

---

## TI-020 — Enable `TYPEIT_WERROR` in CI

**Type** ci · **Size** XS · **Priority** P0 · **Depends on** TI-004, CI-005

The point of the phase. Once the baseline is zero, keep it there.

**Acceptance**
- [x] Every CI job sets `TYPEIT_WERROR=ON`.
- [ ] A deliberately introduced warning fails CI on gcc, clang, and MSVC.
- [x] Local builds still default to warnings-not-errors.

---

## TI-021 — Documentation relocation and README rewrite

**Type** docs · **Size** S · **Priority** P1 · **Depends on** — · **Docs** [REVIEW §8](../CODEBASE_REVIEW.md#8-documentation)

The technical guide currently exists in three places: inlined into `README.md`, and as older
copies at `src/docs/README.md` and `src/docs/TECHNICAL_GUIDE.md`. Three copies, three drift
rates. `src/docs/` is also the wrong home for documentation.

**Scope**
- In: delete `src/docs/`; cut `README.md` back to a real README (what it is, a screenshot,
  quick start, build, links into `docs/`); point `index.md` content at `docs/README.md`.
- Out: rewriting the design docs — they are current.

**Acceptance**
- [x] Exactly one copy of every document.
- [x] `README.md` is under ~120 lines and links to `docs/`.
- [x] No broken relative links (checked by a link-check script or by hand).

---

## TI-022 — Contribution scaffolding

**Type** docs · **Size** S · **Priority** P3 · **Depends on** TI-021

**Scope**
- In: `CONTRIBUTING.md` (build, test, style, commit format, the definition of done);
  GitHub issue templates for bug/feature/task; a PR template with a checklist covering tests,
  CHANGELOG, and docs.
- Out: a code of conduct (single-maintainer project; add if that changes).

**Acceptance**
- [ ] Templates appear when opening an issue or PR.
- [x] `CONTRIBUTING.md` states the definition of done from
      [the backlog README](README.md#definition-of-done).

---

## Phase exit criteria

All of the following, before tagging `v2.0.0-alpha.1`:

- [x] Fresh clone → configure → build → test on Linux **and** Windows, with no submodule step.
      Which is literally what every CI job does — checkout, configure, build, test — and they
      pass.
- [x] Zero warnings with `TYPEIT_WERROR=ON` on gcc, clang, and MSVC. Every matrix job sets it,
      including both Windows compilers.
- [x] All 41 existing tests pass, plus the 3 new ones from TI-005 (65 registered cases).
- [x] Full suite clean under ASan + UBSan.
- [x] CI green on: linux-gcc-debug, linux-gcc-release, linux-clang-debug, sanitizers,
      windows-msvc-debug, windows-msvc-release, windows-clang-cl, format — **and macOS**, which
      was not on this list. First observed whole at `37836da`; before that the Windows jobs
      failed at build for months and then at test, so nothing below them had ever run.
- [x] Exactly one copy of each document; README rewritten.
- [x] `CHANGELOG.md` `Unreleased` block reflects the phase.
