# TypeIt — Review of the Existing Codebase

*Assessment date: 2026-07-30. Reviewed at commit `d7a2d1c`.*

This document is the honest engineering assessment of TypeIt as it stands today, written
before any migration work. It exists so that every decision in
[ARCHITECTURE.md](ARCHITECTURE.md) and [ROADMAP.md](ROADMAP.md) can be traced back to a
concrete problem in the current code rather than to taste.

---

## 1. Summary

| | |
|---|---|
| Implementation | ~1,100 LOC across 21 headers / 15 sources |
| Tests | ~500 LOC, 9 files, 41 test cases |
| Stack | C++20, FTXUI 5, CMake 3.15, vcpkg (git submodule), GoogleTest, GitHub Actions |
| Verdict | **A good learning project with sound instincts and early-career execution.** The layering *intent* is right; the layering *enforcement* is absent. Worth evolving, not worth throwing away. |

The single sentence that explains almost every problem below: **the UI library and a pile of
mutable globals leaked into the domain model, and the domain model updates itself as a side
effect of drawing.** Everything else follows from that.

---

## 2. What is genuinely good

These are not consolation prizes. They are the reason the strangler migration in
[ROADMAP.md](ROADMAP.md) is a better plan than a rewrite.

1. **The directory taxonomy is the right idea.** `core / data / engines / interface / view`
   shows a deliberate attempt to separate pure logic from presentation. The `engines/`
   extraction (`InputAccuracyEngine`, `InputLineEngine`, `InputWordCountEngine`,
   `WordCalculatorEngine`) was clearly a conscious refactor — the commit history says so
   explicitly (`7a6a69a Extracted input line engine from input component`).
2. **`ITextSource` is the correct seam.** A one-method interface for "where does text come
   from" is exactly the extension point the "let me type any text I want" feature needs. It
   survives the migration essentially unchanged in spirit.
3. **The engines have real tests, not smoke tests.** `test_input_line_engine.cpp` covers
   line transitions, backspacing *across* a line boundary, the no-op case, and the
   last-line case. Someone thought about edge cases.
4. **The line/backspace engine is more carefully guarded than it first looks.** I traced the
   boundary conditions: `should_remove_element()` returning `false` when everything is empty
   is what keeps `current_input_line_.pop_back()` from running on an empty vector, and
   `should_finish_game()`'s early return is what keeps an empty text from walking off the end
   of `total_input_lines_`. Those invariants hold. They are *undocumented and accidental*, but
   they hold.
5. **Commit hygiene is better than most hobby projects.** Small, focused, intent-bearing
   messages, including pure-refactor commits. That is a habit worth keeping.
6. **Modern C++ reflexes are present**: `[[nodiscard]]`, `explicit`, `final`, `const` member
   functions, `std::unique_ptr` for the session, `std::move` in constructors, scoped includes.
7. **`.clang-format`, a CI workflow, and a 630-line technical guide exist at all.** For a
   personal project this is well above average.

---

## 3. Architectural problems

### 3.1 Global mutable state is the architecture

`GameState`, `GameOptions`, and `FocusPosition` are structs of `inline static` non-const
members — globals with a namespace. They are read *and written* from every layer:

- `Timer::get_elapsed_time()` writes `GameState::game_finished` (`src/core/Timer.cpp:33`)
- `InputLineEngine::add_element()` writes `GameState::game_finished`
  (`src/engines/InputLineEngine.cpp:55`)
- `InputLineEngine::go_to_new_line()` writes `FocusPosition::y`
  (`src/engines/InputLineEngine.cpp:36`) — a *view* concern mutated from an *engine*
- `ComponentOptions`' render lambdas write `GameOptions::time_radiobox_values_[3]`
  (`include/data/ComponentOptions.h:52`)

**Consequences that are already visible:**
- Tests must reset statics in `SetUp()`. `test_timer.cpp` does it inconsistently and
  `TimerTest.DoesNotCalculateWhenStartGameFalse` depends on whatever the previous test left in
  `GameState::game_session_in_progress`. The suite is order-dependent today.
- Two concurrent sessions, a replay, or a headless simulation are impossible by construction.
- There is no single place to ask "what is the state of the run?"

### 3.2 The model updates itself while rendering

`src/core/Input.cpp:20-31`:

```cpp
ftxui::Component Input::get_input_component() {
    return ftxui::Renderer(input_component_, [&] {
        input_line_.render_input_text(input_text_.empty() ? ' ' : input_text_.back(), input_text_.size());
        input_word_count_.set_word_count(input_text_);
        ...
    });
}
```

Game state advances as a side effect of drawing a frame. Combined with `Screen`'s background
thread posting `Event::Custom` every 100 ms (`src/core/Screen.cpp:14-21`), the whole model is
re-derived ten times a second whether or not the user pressed a key. Word count re-tokenises
the entire input string on every frame.

The same anti-pattern appears in `ComponentOptions::menu_time_input_option`
(`include/data/ComponentOptions.h:50-53`), where a `std::stoi` in a `try`/`catch` with an
**empty catch block** runs inside a render transform.

Render must be a pure function of state. It is not.

### 3.3 FTXUI types live in the domain

`Text` stores `ftxui::Elements`. `InputLineEngine` — nominally an "engine" — stores
`ftxui::Elements`, `std::vector<ftxui::Elements>`, and builds `ftxui::hbox` nodes.

**Consequences:**
- The domain cannot be tested without linking FTXUI, cannot be serialised, and cannot be
  reused.
- Line wrapping is baked into the model at construction time (`Text::populate_text_lines()`),
  so **the app cannot respond to a terminal resize** without rebuilding the model and losing
  the session.
- `InputLineEngine::get_previous_lines_size()` is an `O(n)` `std::accumulate` over every
  previously typed line, and it is called two-to-three times per keystroke per frame. `n` is
  small so it does not matter in practice, but it is a symptom: the code recomputes derived
  state instead of storing it.

### 3.4 Byte-oriented text model

Input is a `std::string` compared `char` by `char`
(`InputLineEngine::get_next_character`). Any multi-byte grapheme desynchronises the accuracy
vector and the line index. The README documents this as an FTXUI limitation:

> FTXUI does not support ćčšđž. When those letters are inputted nothing is shown but
> application calculates it as input…

That is only half true. FTXUI handles UTF-8; the byte-per-character model in `InputLineEngine`
and `Text` is what breaks. A grapheme-cluster–aware text model is a prerequisite for any
non-English text, which directly conflicts with the "type any text you want" goal.

### 3.5 Lifetime-fragile reference plumbing

`Timer::get_elapsed_time_reference()` returns a mutable `int&` to a private member.
`WordCalculator` stores `const int&` to that and to `InputWordCountEngine`'s counter. This
only works because every participant is a member of the same `SpeedTypingSession` aggregate.
It is a hidden coupling that will dangle the first time anything is moved, copied, or
reseated.

---

## 4. Correctness defects

| # | Location | Defect |
|---|---|---|
| C1 | `src/core/FileTextSource.cpp:28` | `content.pop_back()` is unguarded. An existing but **empty** file opens successfully, the read loop never runs, and `pop_back()` on an empty `std::string` is undefined behaviour. Currently masked because `is_file_valid()` rejects empty files upstream — but `get_text()` is public and independently tested. |
| C2 | `include/data/GameOptions.h:17-19` | The bundled corpora are located with `std::filesystem::path current_file_path = __FILE__;` at **runtime**. The binary is not relocatable: it only finds `files/` on the machine that compiled it, with the source tree still at the same absolute path. This is a shipping blocker — the README's `ln -s … /usr/games/TypeIt` step works only by accident. |
| C3 | `src/core/Menu.cpp:29` | `screen_.ExitLoopClosure();` constructs a closure and discards it. The statement is a no-op; only the following `screen_.Exit()` does anything. |
| C4 | `src/engines/InputLineEngine.cpp:76-82` | **Accuracy is unrecoverable.** `remove_element()` never pops from `input_accuracy_`, so backspacing over a mistake and correcting it leaves the error in the denominator *and* adds a new sample. Type `x`, backspace, type the correct `a` → 50% accuracy on a perfectly typed character. Holding backspace and retyping inflates the sample count without bound. |
| C5 | `src/engines/WordCalculatorEngine.cpp:15` | WPM is `60 × (whitespace-delimited words in the user's input) / elapsed`. It counts *incorrect* words, and `Timer::get_elapsed_time()` clamps to `total_time_` once finished. This is not the standard definition (gross WPM = correct characters ÷ 5 ÷ minutes) and the number is not comparable to any other typing test. |
| C6 | `src/core/Text.cpp:47` | `current_character == '\n' \|\| (current_character == ' ' && line.size() >= 55 \|\| index == text_.size() - 1)` mixes `&&` and `\|\|` without parentheses. It happens to parse as intended, but it is exactly the expression `-Wparentheses` exists to flag. The same line compares a signed `int index` against `text_.size()`. |
| C7 | Multiple | `std::size_t` compared against `int` at container boundaries: `current_line_index_ < text_instance_.get_text_lines_size() - 1` and `input_text_size < get_previous_lines_size()`. With an empty text, `get_text_lines_size() - 1` is `-1` converted to `SIZE_MAX`. The out-of-bounds write this would cause is prevented only by `should_finish_game()`'s early return — safety by coincidence, not by design. |
| C8 | `Input`, `Menu`, `Timer` constructors | Member initialiser lists do not match declaration order (`-Wreorder`). Harmless today, but nothing in the build would tell you if it stopped being harmless — see D3. |
| C9 | `src/view/Main.cpp:12` | A `SpeedTypingSession` is constructed at startup, which reads a text file from disk, before the user has chosen anything. |

---

## 5. Build, tooling, and CI

| # | Issue |
|---|---|
| D1 | `CMakeLists.txt:6-15` hardcodes `CMAKE_C_COMPILER`/`CMAKE_CXX_COMPILER` to `gcc`/`g++` (or `cl`). This makes clang, clang-cl, and any user toolchain preference unusable. Compiler selection belongs to the user, via a preset or `CC`/`CXX`. |
| D2 | `CMAKE_TOOLCHAIN_FILE` is `set()` as a normal variable in the top-level list file. It works only because it precedes `project()`, and it cannot be overridden from the command line. It belongs in `CMakePresets.json` or a cache variable. |
| D3 | **No warning flags anywhere.** No `-Wall`, no `-Wextra`, no `/W4`, no `-Werror`, no sanitizers. Every `-Wreorder`, `-Wparentheses`, and `-Wsign-compare` issue listed above is invisible to the build. |
| D4 | `file(GLOB)` in both `src/` and `tests/` — new files do not trigger a reconfigure. |
| D5 | `tests/CMakeLists.txt` **recompiles every application source** into the test binary. There is no shared library target, so everything is built twice. |
| D6 | `tests/CMakeLists.txt` adds `test_input_line_engine.cpp` twice: once via the `./test_*.cpp` glob and once explicitly. |
| D7 | vcpkg is vendored as a **git submodule** (~600 MB clone) for exactly two dependencies, both of which CMake can fetch in a few lines. In this working tree the submodule is not even initialised, so the project does not configure from a fresh clone without `--recursive`. |
| D8 | CI is `ubuntu-latest`, Debug, gcc-14 only. **There is no Windows job**, despite the README shipping Windows build instructions. Cross-platform support is currently unverified. |
| D9 | No install target, no `CMakePresets.json`, no packaging, no `.editorconfig`, no `.clang-tidy`, no format check in CI. |
| D10 | Include paths are relative (`#include "../../include/core/Input.h"`). Moving any file breaks the build; there is no target-based include directory. |
| D11 | Header guards are hand-written and inconsistently named (`INPUT_H` vs `TYPEIT_INPUTACCURACYENGINE_H`). |

---

## 6. Test-suite problems

- **Wall-clock sleeps.** `test_timer.cpp` and `test_screen.cpp` between them `sleep_for`
  roughly 9 seconds. `TimerTest.RemainingTimeStringAfter1Sec` asserts the exact string
  `"9s"` after sleeping 1 second — any scheduling hiccup on a loaded CI runner turns that
  into `"8s"` and a red build. The fix is an injected clock, not a longer sleep.
- **Shared global state** makes the suite order-dependent (see 3.1).
- **No tests at all** for `Menu`, `Main`, `SpeedTypingSession`, `TextInputArea`,
  `PerformanceArea`, or the state machine that connects them. Roughly the entire view and
  control layer is untested — which is itself a consequence of that layer being untestable.
- **No test for C1** (empty file), the one latent UB in the codebase.
- FTXUI can render a component to a string buffer, so snapshot-testing the UI is available
  and unused.

---

## 7. Terminal-portability gaps

The stated goal is Linux and Windows across multiple terminals. Current status:

| # | Issue |
|---|---|
| T1 | `FTXUI_MICROSOFT_TERMINAL_FALLBACK` is branched on in `include/data/ComponentOptions.h:18` but **never defined by any build file**. The Unicode branch (`◉ ○`) is always compiled, and there is no runtime capability detection. |
| T2 | Colours are hardcoded 256-palette entries (`Color::Grey82`, `Color::Salmon1`) with no truecolor/16-colour/monochrome fallback and no `NO_COLOR` support. |
| T3 | Layout uses fixed sizes: `WIDTH EQUAL 75`, `HEIGHT EQUAL 10`, menu `WIDTH EQUAL 20`, and text wraps at a hardcoded 55 characters. Below roughly 80×24 the UI breaks, and terminal resize is not handled at all. |
| T4 | `Ctrl+T` is a poor choice of binding — it is `SIGINFO` on BSD, a tab-management key in several emulators, and collides with tmux/screen prefixes in common configurations. Nothing is rebindable. |
| T5 | No Windows console initialisation is performed by the application (UTF-8 code page, VT processing). FTXUI covers most of this, but it is unverified because there is no Windows CI. |
| T6 | The app delegates text entry to `ftxui::Input` bound to a `std::string`, then reads `input_text_.back()` to find "the character just typed". This is fragile for multi-byte input, IME input, and paste. |

---

## 8. Documentation

The technical guide is genuinely good — accurate, well-organised, and it explains *behaviour*
rather than restating signatures. Two problems:

1. **It is duplicated three ways.** `README.md` now contains the full guide, while
   `src/docs/README.md` and `src/docs/TECHNICAL_GUIDE.md` hold older copies. Three copies
   means three drift rates.
2. **`src/docs/` is the wrong home.** Documentation under a source directory will be
   confusing to every future reader and to any tooling that globs `src/`.

Also: every file carries a `// Created by Kristijan Zalac on 3/8/25.` banner. Git already
stores authorship and dates more accurately than a comment can.

---

## 9. What I actually think

You clearly know *what* good structure looks like — the interface seam, the engine
extraction, the test-first habit on the engines, the refactoring commits. What is missing is
the enforcement that turns intent into structure: a build that fails on warnings, targets
whose include paths make illegal dependencies impossible to write, and a domain model that
does not know the UI library exists. Those three things are what separate this from a
production codebase, and none of them require you to become a different programmer — they are
mechanical.

The two decisions I would call actual mistakes rather than inexperience are the **globals**
(they are load-bearing here, and everything downstream inherits their problems) and
**`__FILE__` for asset lookup** (it silently makes the binary undistributable, which is a
strange outcome for a project that has installation instructions).

The two things I would call genuinely impressive for the stage are the **boundary conditions
in `InputLineEngine`** — they really do hold, and I checked each one — and the **technical
guide**, which is better than the documentation on plenty of professional projects.

**Recommendation: strangler migration, not rewrite.** Around 40% of the current logic
(text wrapping rules, the line/backspace state machine's behaviour, file validation, word
counting) is behaviour worth preserving; the existing tests are the specification for it. The
remaining 60% is plumbing that has to go regardless. Building the new core alongside the old
app keeps that specification executable at every step. See [ROADMAP.md](ROADMAP.md).

---

## 10. Traceability

Every finding above is addressed by a specific phase of the plan:

| Finding | Addressed by |
|---|---|
| 3.1 globals | ADR-005, Phase 1 |
| 3.2 render side effects | ADR-003, Phase 1 |
| 3.3 FTXUI in domain | ADR-001, Phase 1 |
| 3.4 byte model | ADR-004, Phase 1 |
| 3.5 reference plumbing | ADR-005, Phase 1 |
| C1, C6, C7, C8, C9 | Phase 0 (warnings) + Phase 1 (rewrite) |
| C2 asset lookup | ADR-006, Phase 2 |
| C3 no-op exit | Phase 4 |
| C4 accuracy | ADR-002, Phase 1 |
| C5 WPM definition | [GAMEPLAY.md §4](GAMEPLAY.md), Phase 1 |
| D1–D11 | Phase 0 |
| §6 tests | ADR-005 + [TESTING.md](TESTING.md), Phases 0–1 |
| T1–T6 | ADR-011 + [UX.md §6](UX.md), Phases 4 and 9 |
| §8 docs | Phase 0 |
