# TypeIt — Build System, Dependencies, and CI

*How the project is configured, built, tested, packaged, and verified on both platforms. This
replaces the build described in [review §5](CODEBASE_REVIEW.md#5-build-tooling-and-ci).*

---

## 1. What is wrong today, and the fix

| Problem | Fix |
|---|---|
| `CMakeLists.txt` hardcodes `CMAKE_C_COMPILER`/`CMAKE_CXX_COMPILER` to `gcc`/`cl` | Compiler comes from presets or `CC`/`CXX` |
| `CMAKE_TOOLCHAIN_FILE` set as a normal variable in the top-level list file | Toolchain lives in `CMakePresets.json`, overridable |
| No warning flags at all | `cmake/CompilerWarnings.cmake`, warnings-as-errors in CI |
| `file(GLOB)` for sources | Explicit source lists |
| Tests recompile every application source | Libraries are real targets; tests link them |
| `test_input_line_engine.cpp` listed twice | Explicit lists make duplication visible |
| vcpkg vendored as a ~600 MB submodule for two dependencies | `FetchContent` + `find_package` fallback (ADR-007) |
| CI is Linux/Debug/gcc-14 only | Matrix: Linux gcc + clang, Windows MSVC, Debug + Release, sanitizers |
| No install target, no packaging, no presets | Install rules, CPack, `CMakePresets.json` |
| Relative `../../include/...` includes | Target `PUBLIC` include directories |

---

## 2. Requirements

| | Minimum | Notes |
|---|---|---|
| CMake | 3.24 | `FetchContent` `FIND_PACKAGE_ARGS` requires 3.24 |
| Ninja | any recent | Recommended generator on both platforms |
| GCC | 13 | `<expected>` |
| Clang | 17 with libc++, **19** with libstdc++ | Clang below 19 reports `__cpp_concepts` as 201907, and libstdc++ gates `std::expected` on 202002: the header includes and the namespace is empty. CI builds clang against libc++ |
| MSVC | 19.38 (VS 2022 17.8) | |
| Git | any | |

> **On this machine right now:** GCC 16.1.1 is present, but **`cmake` and `ninja` are not
> installed**. On Arch: `sudo pacman -S cmake ninja`. Nothing else is needed — the dependency
> strategy below fetches what it cannot find.

---

## 3. Project structure

```
CMakeLists.txt                  project(), options, add_subdirectory
CMakePresets.json               configure/build/test presets
cmake/
├─ Dependencies.cmake           FetchContent + find_package fallback
├─ CompilerWarnings.cmake       typeit_set_warnings(target)
├─ Sanitizers.cmake             typeit_enable_sanitizers(target)
├─ StaticAnalysis.cmake         clang-tidy, cppcheck, IWYU (opt-in)
└─ Install.cmake                install rules + CPack
libs/
├─ core/   CMakeLists.txt  →  typeit::core
├─ app/    CMakeLists.txt  →  typeit::app
├─ infra/  CMakeLists.txt  →  typeit::infra
├─ tui/    CMakeLists.txt  →  typeit::tui
└─ cli/    CMakeLists.txt  →  typeit::cli
apps/typeit/CMakeLists.txt  →  typeit (executable)
tests/     core/ app/ infra/ tui/ e2e/
assets/    texts/ themes/
```

### Targets and the dependency rule

Layering from [ARCHITECTURE §2](ARCHITECTURE.md#the-dependency-rule) is enforced by what each
target links, which controls which include directories are visible:

```cmake
# libs/core/CMakeLists.txt
add_library(typeit_core STATIC ${TYPEIT_CORE_SOURCES})
add_library(typeit::core ALIAS typeit_core)
target_include_directories(typeit_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_compile_features(typeit_core PUBLIC cxx_std_23)
typeit_set_warnings(typeit_core)
# NOTHING else is linked. FTXUI and SQLite headers are not reachable from here.

# libs/tui/CMakeLists.txt
target_link_libraries(typeit_tui PUBLIC typeit::core typeit::app PRIVATE ftxui::component …)
#                                                                ^ PRIVATE: FTXUI does not
#                                                                  leak to tui's consumers
```

Because `typeit_core` never links FTXUI, `#include <ftxui/…>` inside `core` is a *compile
error*, not a review comment. That is the entire enforcement mechanism, and it costs nothing.

---

## 4. Dependencies (ADR-007)

`cmake/Dependencies.cmake`:

```cmake
include(FetchContent)

set(FETCHCONTENT_QUIET OFF)

# Prefer a system/distro/vcpkg package; otherwise download and build.
FetchContent_Declare(ftxui
    GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI.git
    GIT_TAG        v6.1.9            # pinned; bumped deliberately
    FIND_PACKAGE_ARGS 5.0 CONFIG NAMES ftxui)

FetchContent_Declare(tomlplusplus
    GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
    GIT_TAG        v3.4.0
    FIND_PACKAGE_ARGS CONFIG NAMES tomlplusplus)

# find_package(SQLite3) is a CMake builtin module, so the declaration below is
# only reached when no SQLite is installed. The amalgamation ships no CMake
# project, so the target is declared by hand (see cmake/Dependencies.cmake).
FetchContent_Declare(sqlite_amalgamation
    URL      https://www.sqlite.org/2026/sqlite-amalgamation-3530400.zip
    URL_HASH SHA3_256=628a44cfe82c66aed1ccbbe85a562d2e33ebe64b3288981ed76285612227934e)

FetchContent_MakeAvailable(ftxui tomlplusplus)

if(TYPEIT_BUILD_TESTS)
    FetchContent_Declare(googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        v1.17.0
        FIND_PACKAGE_ARGS NAMES GTest)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)   # required on Windows
    FetchContent_MakeAvailable(googletest)
endif()
```

**What this buys.** One code path serves everyone:

| User | What happens |
|---|---|
| Arch/Debian user with distro packages | `find_package` succeeds; nothing is downloaded |
| vcpkg user | Configure with the vcpkg toolchain; `find_package` succeeds via vcpkg |
| Fresh clone, nothing installed | FetchContent downloads and builds the pinned versions |
| Air-gapped / packager | `-DFETCHCONTENT_FULLY_DISCONNECTED=ON` with pre-populated sources |

**vcpkg.** `vcpkg.json` is **kept** so vcpkg remains a supported opt-in, and
`vcpkg-configuration.json` keeps the baseline pinning. **The `external/vcpkg` submodule and
`.gitmodules` are removed.** Nothing in the build requires vcpkg any more; it is one of four
equal paths above.

When the amalgamation URL needs pinning by hash, `URL_HASH SHA3_256=…` is added — every
downloaded archive is hash-pinned, every git dependency is tag-pinned. No floating refs.

### Dependency inventory

| Dependency | Layer | Required? | For |
|---|---|---|---|
| FTXUI | `tui` | yes (unless `TYPEIT_BUILD_TUI=OFF`) | Terminal rendering |
| SQLite3 | `infra` | yes | History and library |
| toml++ | `infra` | yes | Configuration and themes |
| GoogleTest | tests | test builds only | — |
| miniz | `infra` | `TYPEIT_ENABLE_EPUB` | EPUB/DOCX ZIP containers |
| pugixml *or* lexbor | `infra` | `TYPEIT_ENABLE_EPUB` | OPF and XHTML parsing; HTML extraction |
| libcurl | `infra` | `TYPEIT_ENABLE_NETWORK` | Web import ([TX-010](issues/PHASE-6A-text-sources.md#tx-010--http-fetcher)) |

The last three are optional by design. `TYPEIT_ENABLE_NETWORK=OFF` is not a degraded build — it
is the configuration a distro packager or a security-conscious user should be able to choose,
and it makes "nothing phones home" verifiable from the link line rather than from a promise.
CI builds both configurations.

Heavy formats such as PDF are **never** bundled; they route through the user's own converters
([ADR-015](ARCHITECTURE.md#adr-015--heavy-formats-use-an-external-converter-hook-not-a-bundled-parser)).

---

## 5. Options

| Option | Default | Effect |
|---|---|---|
| `TYPEIT_BUILD_TESTS` | `ON` when top-level | Build and register tests |
| `TYPEIT_BUILD_TUI` | `ON` | Build the terminal frontend (off = core/app/infra only) |
| `TYPEIT_ENABLE_NETWORK` | `ON` | Build the HTTP fetcher ([ADR-014](ARCHITECTURE.md#adr-014--network-access-is-optional-at-build-time-and-opt-in-at-runtime)). **Off = libcurl is not a dependency at all**, and the resulting binary contains no networking symbols |
| `TYPEIT_ENABLE_EPUB` | `ON` | Build the EPUB extractor (miniz + XML parser) |
| `TYPEIT_WERROR` | `OFF` (`ON` in CI) | Warnings as errors |
| `TYPEIT_SANITIZERS` | `OFF` | `address;undefined` on Linux/macOS Debug |
| `TYPEIT_CLANG_TIDY` | `OFF` | Run clang-tidy during compilation |
| `TYPEIT_IWYU` | `OFF` | Run include-what-you-use |
| `TYPEIT_COVERAGE` | `OFF` | `--coverage` instrumentation |
| `TYPEIT_INSTALL` | `ON` when top-level | Generate install rules |

---

## 6. Warnings

`cmake/CompilerWarnings.cmake`:

```cmake
function(typeit_set_warnings target)
    set(gcc_clang
        -Wall -Wextra -Wpedantic
        -Wshadow -Wnon-virtual-dtor -Wold-style-cast -Wcast-align
        -Wunused -Woverloaded-virtual -Wconversion -Wsign-conversion
        -Wnull-dereference -Wdouble-promotion -Wimplicit-fallthrough
        -Wmisleading-indentation)
    set(gcc_only -Wduplicated-cond -Wduplicated-branches -Wlogical-op -Wuseless-cast)
    set(msvc /W4 /permissive- /w14640 /w14826 /w14928 /utf-8)
    ...
    if(TYPEIT_WERROR)
        target_compile_options(${target} PRIVATE $<IF:$<CXX_COMPILER_ID:MSVC>,/WX,-Werror>)
    endif()
endfunction()
```

`/utf-8` on MSVC is required, not optional — the source contains UTF-8 string literals and
MSVC otherwise interprets them in the system codepage.

This set catches, on the very first build, every latent issue found in
[review §4](CODEBASE_REVIEW.md#4-correctness-defects): `-Wreorder` on the `Input`, `Menu`, and
`Timer` constructors; `-Wparentheses` on `Text.cpp:47`; `-Wsign-compare` and
`-Wsign-conversion` throughout the line engine.

---

## 7. Presets

`CMakePresets.json` (version 6). Configure presets:

| Name | Generator | Compiler | Build type | Extras |
|---|---|---|---|---|
| `linux-gcc-debug` | Ninja | gcc | Debug | tests, `TYPEIT_WERROR=ON` |
| `linux-gcc-release` | Ninja | gcc | RelWithDebInfo | install rules |
| `linux-clang-debug` | Ninja | clang | Debug | tests |
| `linux-clang-asan` | Ninja | clang | Debug | ASan + UBSan |
| `linux-tidy` | Ninja | clang | Debug | `TYPEIT_CLANG_TIDY=ON` |
| `linux-coverage` | Ninja | gcc | Debug | `TYPEIT_COVERAGE=ON` |
| `windows-msvc-debug` | Ninja | cl | Debug | tests |
| `windows-msvc-release` | Ninja | cl | RelWithDebInfo | install rules |
| `windows-clang-cl` | Ninja | clang-cl | Debug | tests |
| `vcpkg` | Ninja | default | Debug | inherits the vcpkg toolchain from `$env{VCPKG_ROOT}` |

Everyday use:

```bash
cmake --preset linux-gcc-debug
cmake --build --preset linux-gcc-debug
ctest --preset linux-gcc-debug
```

```powershell
cmake --preset windows-msvc-debug
cmake --build --preset windows-msvc-debug
ctest --preset windows-msvc-debug
```

That is the whole build story from a fresh clone, with no submodule step — compare with the
current README, which requires `git submodule update --init --recursive` first and still fails
if it is skipped.

---

## 8. Install and packaging

```cmake
install(TARGETS typeit RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY assets/texts/  DESTINATION ${CMAKE_INSTALL_DATADIR}/typeit/texts)
install(DIRECTORY assets/themes/ DESTINATION ${CMAKE_INSTALL_DATADIR}/typeit/themes)
install(FILES docs/typeit.1 DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)   # Linux only
```

The install prefix is baked in at configure time and used as one entry of the asset search
path described in [TECHNICAL §4.1](TECHNICAL.md#41-platform-paths). Together with the
executable-relative lookup, this makes the binary **relocatable** — the direct fix for
[defect C2](CODEBASE_REVIEW.md#4-correctness-defects), where assets are found via `__FILE__`
and the program therefore only works on the machine that built it.

CPack produces `TGZ` and `DEB` on Linux and `ZIP` plus an optional WiX/NSIS installer on
Windows. Release artefacts are built by CI on tag push.

---

## 9. Continuous integration

**The pipeline has its own document: [CI_CD.md](CI_CD.md).** It is built *before* the rebuild
starts ([Phase 0A](issues/PHASE-0A-cicd.md)), against the codebase as it stands today.

What matters from this document's point of view is only what CI expects of the build system:

- Every preset in §7 is invoked by a job, so a broken preset is caught immediately.
- `FETCHCONTENT_BASE_DIR` is cacheable and keyed on the dependency pin hashes (§4).
- `TYPEIT_WERROR`, `TYPEIT_SANITIZERS`, `TYPEIT_COVERAGE`, `TYPEIT_CLANG_TIDY`,
  `TYPEIT_ENABLE_NETWORK`, and `TYPEIT_ENABLE_EPUB` are all settable from the command line, so
  CI never needs a bespoke configuration path.
- `ctest --output-junit` produces machine-readable results.
- `actions/checkout` no longer needs `submodules: recursive` (§4).

**The Windows jobs are the important addition.** The project currently ships Windows build
instructions with zero automated verification that they work.

---

## 10. Local developer workflow

```bash
# One-time
sudo pacman -S cmake ninja            # Arch; this machine currently lacks both

# Everyday
cmake --preset linux-gcc-debug
cmake --build --preset linux-gcc-debug
ctest --preset linux-gcc-debug --output-on-failure

# Before pushing
cmake --build --preset linux-clang-asan && ctest --preset linux-clang-asan
clang-format -i $(git diff --name-only --cached | grep -E '\.(h|cpp)$')

# Run without installing
./build/linux-gcc-debug/apps/typeit/typeit
```

`compile_commands.json` is emitted by every Ninja preset (`CMAKE_EXPORT_COMPILE_COMMANDS=ON`)
for clangd and clang-tidy.

---

## 11. Migration checklist for Phase 0

Ordered so the tree builds at every step:

1. Add `cmake/CompilerWarnings.cmake`; apply to the existing targets **without** `-Werror`.
   Record the warning baseline — expect `-Wreorder` × 3, `-Wparentheses` × 1, and a run of
   sign-compare warnings.
2. Add `cmake/Dependencies.cmake`; switch `find_package(ftxui)` / `find_package(GTest)` to go
   through it.
3. Remove `external/vcpkg` from the index and delete `.gitmodules`. Keep `vcpkg.json` and
   `vcpkg-configuration.json`.
4. Delete the compiler and toolchain `set()` calls from the top-level `CMakeLists.txt`.
5. Add `CMakePresets.json`.
6. Replace both `file(GLOB)` calls with explicit lists; drop the duplicated
   `test_input_line_engine.cpp`.
7. Extract the current sources into one interim `typeit_legacy` library so the test target
   links it instead of recompiling everything.
8. Add the CI matrix, including the first-ever Windows job. Expect it to fail initially —
   fixing it is part of the phase.
9. Add `.editorconfig`, `.clang-tidy`, and the format check.
10. Fix the warning baseline to zero, then turn on `TYPEIT_WERROR` in CI.
11. Move documentation to `docs/`, delete the duplicate copies under `src/docs/`, and trim the
    README to a real README.

At the end of Phase 0 the application behaves identically, but every subsequent change is
built on a foundation that reports its own problems.
