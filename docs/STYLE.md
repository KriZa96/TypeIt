# TypeIt — Coding Style and Conventions

*The rules the code follows. Where a rule differs from what the current codebase does, that is
called out explicitly so the change is a decision rather than a drift.*

Everything mechanical here is enforced by `.clang-format`, `.clang-tidy`, and compiler
warnings — not by review. Review is for the things a tool cannot check.

---

## 1. Language and formatting

- **C++23.** Compiler floor in [ARCHITECTURE §6.1](ARCHITECTURE.md#61-language-level-and-compiler-floor).
- **Formatting is `.clang-format`'s job.** The existing config (Google base, 4-space indent,
  120 columns, custom brace wrapping) is kept as-is. Do not hand-format; do not argue about
  formatting in review. CI runs `clang-format --dry-run --Werror`.
- `.editorconfig` covers non-C++ files: UTF-8, LF line endings, final newline, no trailing
  whitespace.

---

## 2. Naming

| Kind | Convention | Example |
|---|---|---|
| Type, concept, enum | `PascalCase` | `TypingModel`, `GraphemeState` |
| Interface | `I` + `PascalCase` | `ITextProvider`, `IHistoryRepository` |
| Function, method, variable | `snake_case` | `compute_metrics`, `line_width` |
| Private data member | `snake_case_` | `cursor_`, `text_buffer_` |
| Compile-time constant | `k` + `PascalCase` | `kDefaultLineWidth` |
| Enumerator (scoped enum) | `PascalCase` | `GraphemeState::Corrected` |
| Namespace | `snake_case` | `typeit::core::text` |
| Macro | avoid; if unavoidable, `TYPEIT_SCREAMING` | `TYPEIT_ASSERT` |
| File | `PascalCase.h` / `.cpp`, named for its primary type | `TypingModel.h` |
| Test file | `<Unit>Test.cpp` | `TypingModelTest.cpp` |

This keeps the existing project conventions (snake_case methods, trailing-underscore members,
`I`-prefixed interfaces, PascalCase files) — they are consistent today and worth preserving.

**Changed from current:** test files move from `test_foo.cpp` to `FooTest.cpp` so a test sits
alphabetically next to what it tests, and constants gain the `k` prefix (the current code has
no constants convention because it has no named constants — the `55`, `75`, `10`, and `100`
scattered through `Text.cpp`, `Style.h`, and `Screen.cpp` are all magic numbers).

**Naming rules that are not about case:**

- Names say *what*, not *how*: `wrap()`, not `calculate_line_breaks_with_greedy_algorithm()`.
- No abbreviations except universally understood ones (`id`, `db`, `utf8`, `wpm`).
- Booleans read as predicates: `is_finished()`, `has_bookmark()`, `should_wrap()`.
- Getters do not carry a `get_` prefix when they are trivial accessors — `cursor()`, not
  `get_cursor()`. *(This is a deliberate departure from the current `get_text()` /
  `get_word_count_reference()` style: a name like `get_word_count_reference` describes the
  implementation, and the implementation is exactly what we are removing.)*
- Functions that can fail return `Result<T>`; their names do not need `try_`.

---

## 3. Headers and includes

- **`#pragma once`.** No hand-written guards. (Current guards are inconsistently named:
  `INPUT_H` next to `TYPEIT_INPUTACCURACYENGINE_H`.)
- **Target-qualified includes only.** `#include "typeit/core/session/Session.h"`. Relative
  paths like `../../include/core/Input.h` are forbidden — they are what makes a file
  impossible to move and a layer violation impossible to detect.
- Include order, enforced by `.clang-format`:
  1. The header this `.cpp` implements
  2. C++ standard library, `<angle>`
  3. Third-party, `<angle>`
  4. TypeIt, `"quoted"`
- **Include what you use.** No relying on transitive includes. `include-what-you-use` runs as
  an opt-in CI job.
- Prefer forward declarations in headers; include in the `.cpp`.
- Headers declare; they do not define non-inline, non-template functions.
- No `using namespace` at namespace scope in a header, ever. Inside a `.cpp`, a narrow
  `using ftxui::Element;` is fine.

---

## 4. Types and ownership

- **Rule of zero.** Write destructors, copy, and move only when the class actually manages a
  resource. If you write one, write all five or `= delete` the rest.
- **Ownership is explicit:**
  - `std::unique_ptr<T>` — sole ownership
  - `std::shared_ptr<T>` — genuine shared ownership only. FTXUI forces `shared_ptr` at its
    API boundary; that is **confined to `typeit::tui`** and does not leak inward.
  - `T&` / `const T&` — non-owning, non-null, outlives the callee
  - `T*` — non-owning and nullable, and the nullability is documented
  - `std::span<const T>` / `std::string_view` — non-owning views for parameters
- **Never return a reference to a private member for the caller to hold.** The current
  `Timer::get_elapsed_time_reference()` returning `int&`, with `WordCalculator` storing it, is
  the pattern this rule exists to prevent.
- `enum class` always; plain `enum` never.
- `const` by default: member functions, locals, parameters taken by reference.
- `[[nodiscard]]` on every function whose return value is the point of calling it.
- `explicit` on every single-argument constructor.
- `final` on classes not designed for inheritance.
- Strong types for domain quantities (ADR-010) — no bare `int` for a WPM, an index, or a
  duration.

---

## 5. Functions

- One job per function. If you need "and" to describe it, split it.
- Soft limits: **40 lines**, **4 parameters**, **cyclomatic complexity 10**. `clang-tidy`
  flags violations; exceeding a limit needs a comment saying why.
- Prefer free functions in a namespace over static member functions.
- Prefer pure functions. Every function in `core/metrics/` and `core/text/Wrapper.h` is pure,
  and that is what makes them cheap to test exhaustively.
- Out-parameters are forbidden; return a struct or `std::tuple`.
- Default arguments are avoided in virtual functions (they do not participate in overriding).

---

## 6. Error handling

Per ADR-009:

- `Result<T>` / `Status` for anything a user can cause — missing file, bad config, corrupt
  database, invalid keybinding.
- Exceptions only for broken invariants, and they never cross a layer boundary. `main` has one
  top-level handler.
- `TYPEIT_ASSERT(cond, msg)` for internal invariants; active in debug, compiled out in release.
- **`catch (...) {}` is banned.** So is any empty catch block. The current
  `catch (const std::invalid_argument& _) {}` inside a render transform is the exact pattern
  this rule exists to eliminate.
- Never log and rethrow. Handle it, or propagate it.
- Error messages are written for the user: what failed, which file, what to do about it.

```cpp
// Good
Result<TextBuffer> load(const std::filesystem::path& path) {
    auto bytes = fs_.read_text(path);
    if (!bytes) {
        return std::unexpected(Error{ErrorCode::FileUnreadable,
                                     std::format("Cannot read '{}'.", path.string()),
                                     bytes.error().context});
    }
    return TextBuffer::from_utf8(*bytes);
}
```

---

## 7. State and dependencies

- **No mutable global or namespace-scope state.** `static` is permitted only for `constexpr`
  data. The deletion of `GameState`, `GameOptions`, and `FocusPosition` is not negotiable
  ([review §3.1](CODEBASE_REVIEW.md#31-global-mutable-state-is-the-architecture)).
- Dependencies arrive through constructors. No service locator, no singleton, no static
  registry.
- Anything non-deterministic — time, randomness, the filesystem, the database — comes in
  behind an interface so tests can substitute it.
- A class either holds state or coordinates; try not to do both.

---

## 8. Concurrency

- Document the thread affinity of every class that has one, in the class comment.
- The domain model is **single-threaded by contract** (ADR-012). It contains no mutex, no
  atomic, no thread.
- The only shared mutable state in the whole application is the frame ticker's stop flag and
  interval, both `std::atomic`.
- Threads are owned by RAII types that stop and join in their destructor.

---

## 9. Comments and documentation

- Comments explain **why**, not what. If a comment restates the code, delete one of them.
- Every public class gets a short block: what it is for, its invariants, its thread affinity.
- Every non-obvious algorithm cites its source — the ramp law in `DifficultyController` points
  at [GAMEPLAY §3.2](GAMEPLAY.md#32-the-ramp-law), the WPM formulas point at §4.
- Mark deliberate subtleties. If a boundary condition is load-bearing, say so — the current
  `InputLineEngine` has three such conditions and documents none of them, which is why they
  read as accidents.
- `TODO` must reference an issue: `// TODO(#42): …`. A bare TODO is a lie.
- **No file-header banners.** `// Created by X on 3/8/25.` is metadata git already stores more
  accurately. New files start with `#pragma once` or the first include.

---

## 10. Testing conventions

Detail in [TESTING.md](TESTING.md); the style rules:

- One behaviour per test. The name states it: `TypingModelTest, BackspaceOverCorrectedErrorRestoresPendingState`.
- Arrange / Act / Assert, with blank lines between the three.
- **No `sleep_for` in any test.** Inject `FakeClock`. The current suite sleeps roughly nine
  seconds and contains at least one assertion (`"9s"` after a one-second sleep) that is a race
  against the scheduler.
- No shared mutable state between tests. Fixtures construct fresh objects.
- Test the public interface. `FRIEND_TEST` is a last resort and needs a comment justifying it.
- Prefer table-driven tests for anything with many cases (segmentation, wrapping,
  normalisation).
- Assert on values, not on log output.

---

## 11. Commits and branches

- **Conventional Commits**: `feat:`, `fix:`, `refactor:`, `test:`, `docs:`, `build:`, `ci:`,
  `perf:`, `chore:`, with an optional scope — `feat(race): add difficulty controller`.
- Subject in the imperative mood, ≤ 72 characters, no trailing period.
- The body explains *why*. The diff already shows what.
- One logical change per commit. Refactors are separate commits from behaviour changes —
  a habit the current history already demonstrates and should keep.
- Branch names: `feat/<slug>`, `fix/<slug>`, `refactor/<slug>`.
- Every commit on `main` must build clean with warnings-as-errors and pass all tests.

---

## 12. What CI enforces

| Gate | Tool |
|---|---|
| Formatting | `clang-format --dry-run --Werror` |
| Warnings | `-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion -Wold-style-cast -Wnon-virtual-dtor -Wnull-dereference -Wdouble-promotion` + `-Werror`; `/W4 /permissive- /WX` on MSVC |
| Static analysis | `clang-tidy` with the project `.clang-tidy` |
| Memory / UB | ASan + UBSan on the Linux Debug job |
| Tests | `ctest --output-on-failure` on every platform in the matrix |
| Layering | Each library is a separate target; illegal includes fail to compile |

The warning set is chosen to catch what is actually wrong in the current code today:
`-Wreorder` (three constructors), `-Wparentheses` (`Text.cpp:47`), and `-Wsign-compare` /
`-Wsign-conversion` (throughout the line engine). Turning these on is the single highest-value
change in Phase 0.
