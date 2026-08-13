# Phase 4 — `typeit::tui` and cutover

**Milestone:** `v2.0.0-alpha.5` · **Issues:** TI-079 – TI-098 · **Goal:** feature parity with
1.0.0 on the new architecture, then delete the old code.

This phase ends with `git rm` on the entire legacy tree. That only happens once parity is
demonstrated against a written checklist (TI-098), and the checklist is the definition of done
— **anything not on it waits for Phase 5**. Merging the cutover with new features is how
migrations stall.

---

## TI-079 — Scaffold `typeit_tui` and `TerminalApp`

**Type** build · **Size** M · **Priority** P0 · **Depends on** TI-066, TI-007 · **Docs** [ARCHITECTURE §4.4](../ARCHITECTURE.md#44-typeittui--driving-adapter)

**Scope**
- In: `libs/tui` linking `typeit::core`, `typeit::app`, and FTXUI **`PRIVATE`**; `TerminalApp`
  owning `ScreenInteractive` and the event loop; `tests/tui` target.
- Out: screens and widgets.

**Tests**
- Negative build test: including an `infra` or SQLite header from `tui` fails to compile.
- `TerminalApp` constructs and shuts down cleanly with a stub screen.

**Acceptance**
- [x] `tui` cannot see `infra`; the composition root is the only place both are visible.
      Enforced twice: `infra.lint.source_rules` now forbids `typeit/infra/` inside `libs/tui`,
      and `tui.isolation.infra_is_unreachable` is a target that must fail to compile — with a
      control beside it, because a negative test that cannot tell "unreachable" from "the
      compiler never ran" proves nothing.
- [x] FTXUI is `PRIVATE`, so it does not leak to `tui`'s consumers. Two guarantees rather than
      one: the link is private so no include directory is inherited, and `TerminalApp.h` names
      nothing from FTXUI — not a type, not an include — so a consumer could not use them if it
      had them. `tui.isolation.ftxui_does_not_leak` is the check.
- [x] The event loop is not entered by any test. FTXUI's loop reads the terminal, and a test
      that starts it in CI is a test that hangs a pipeline the day stdin behaves differently.
      The shutdown path is reachable without it: `quit()` before `run()` is honoured without
      entering the loop at all, which is the only sane answer to "stop" arriving first.

---

## TI-080 — `FrameTicker`

**Type** feat · **Size** S · **Priority** P0 · **Depends on** TI-079 · **Docs** [ADR-012](../ARCHITECTURE.md#adr-012--one-frame-ticker-adaptive-rate-single-threaded-model)

Replaces `Screen`'s fixed 100 ms poll thread. One thread, adaptive interval (~60 ms active,
~500 ms idle), RAII stop-and-join. Its **only** job is posting an event; it never touches model
state.

**Unit tests** (`FrameTickerTest.cpp`)
- Construction starts the thread; destruction stops and joins it — no leak under ASan/TSan.
- `set_active` changes the interval, verified by counting posted events over a window against a
  tolerance.
- Destruction while a post is in flight is safe.
- Repeated construct/destruct (100 cycles) leaks nothing.
- **Ported from `test_screen.cpp`**, all five cases, **without any `sleep_for`**: use a
  countdown latch on posted events rather than wall-clock waits.

**Acceptance**
- [x] No `sleep_for` in the tests — the word appears once, in a comment saying so. Every wait
      is a condition variable on posted events, which is both exact and instant: the file runs
      in **70–80 ms** against the three seconds the five `ScreenTest` cases spent sleeping.
      Not the "well under 100 ms" this issue asked for, and the remainder is inherent: a
      hundred thread create/join cycles cost about 20 ms of pure OS work, and watching an
      interval lengthen means waiting out one lengthened interval. The intervals are injectable
      so the tests choose milliseconds rather than the shipped 60/500.
- [x] The ticker holds no reference to any model type. It holds a `std::function<void()>` and
      nothing else; the header names no domain type at all.
- [x] Verified under TSan as well as ASan — this is the first thread in the rebuild, which is
      what the nightly TSan job was put there for. `tui_tests` is clean with
      `-fsanitize=thread -fno-sanitize-recover=all`. The nightly job itself cannot run yet and
      will not link when it does, for a reason that predates this issue: see the note under
      [CI-013](PHASE-0A-cicd.md#ci-013--nightly-workflow).
- [x] `FrameIntervals` is at namespace scope rather than nested in `FrameTicker`. A nested
      type's default member initialisers are not parsed until the enclosing class is complete,
      so `Intervals intervals = {}` as a default argument inside the class body is ill-formed —
      accepted by gcc, rejected by clang, and therefore a CI failure rather than a local one.

---

## TI-081 — `ScreenStack` and `IScreen`

**Type** feat · **Size** M · **Priority** P0 · **Depends on** TI-079 · **Docs** [TECHNICAL §3.2](../TECHNICAL.md#32-screen-stack)

Navigation as an explicit stack, replacing the five interacting booleans in the current
`GameState` (`game_session_in_progress`, `start_session`, `refresh_session`, `game_finished`,
`show_info`).

**Unit tests** (`ScreenStackTest.cpp`)
- Push/pop/replace change the top as expected.
- **Only the top screen receives events**, verified with spy screens.
- Popping the last screen is refused (or exits, per the documented choice).
- A screen's lifetime ends when popped — no dangling reference.
- Deep nesting (10 screens) works and unwinds correctly.
- Push during event handling is deferred to the end of the frame, not applied mid-dispatch.

**Acceptance**
- [x] No boolean navigation flag exists anywhere in `tui`. Where the program is has exactly one
      representation — the stack — and nothing else can disagree with it. The five booleans
      that remain in the layer are not navigation: `dispatching_` is a re-entrancy guard for
      one function, `quitting` is the application's lifecycle, and the ticker's three are
      thread and rate state.
- [x] Popping the last screen is **refused**, which is the documented choice this issue leaves
      open. An empty stack renders nothing and answers no key, so a program that popped its
      last screen would be a black terminal that ignores the keyboard. Leaving is
      `TerminalApp::quit()`, which says so.
- [x] `ScreenStack` and `IScreen` are internal to the library, not public headers: they name
      `ftxui::Element` and `ftxui::Event`, and TI-079's guarantee is that FTXUI stops at this
      library's edge. `tests/tui` reaches into `libs/tui/src` deliberately — that is what a
      white-box test of a library's internals is — and the isolation probes still link
      `typeit::tui` alone.
- [x] `IScreen` uses snake_case, unlike TECHNICAL §3.2's original sketch. Those were FTXUI's
      names, correct on `TypingArea` because it overrides `ftxui::ComponentBase`; `IScreen`
      overrides nothing, so STYLE.md applies. The sketch has been updated to match rather than
      left to contradict the code.

---

## TI-082 — `Capabilities::detect()`

**Type** feat · **Size** M · **Priority** P0 · **Depends on** TI-079 · **Docs** [UX §6.2](../UX.md#62-capability-detection)

Runtime detection of colour depth and glyph set, replacing the
`FTXUI_MICROSOFT_TERMINAL_FALLBACK` macro that is
[branched on but never defined by any build file](../CODEBASE_REVIEW.md#7-terminal-portability-gaps)
— so the Unicode branch always compiles and the fallback has never once executed.

**Unit tests** (`CapabilitiesTest.cpp`) — environment injected, table-driven over the full
precedence order
- `NO_COLOR` set (even empty) → `Mono`, and it beats everything else.
- `TYPEIT_COLOR` / `TYPEIT_GLYPHS` override detection.
- `COLORTERM=truecolor` and `=24bit` → `TrueColor`.
- `WT_SESSION` present → `TrueColor` + `Unicode`.
- `TERM=xterm-256color` → `Ansi256`.
- `TERM=linux` and `TERM=dumb` → `Ansi16` + `Ascii`.
- Nothing set → `Ansi16` + `Ascii` (the conservative floor).
- Config overrides detection in every case.
- Detection records **why** it concluded what it did, for `--doctor`.

**Acceptance**
- [x] Precedence order matches UX §6.2 exactly, row by row. **Landed early, in TI-076**:
      `--doctor` reports the detected colour depth and glyph set *with the reason*, so the
      detection had to exist in Phase 3. It lives in `infra::detect_capabilities`
      (`typeit/infra/term/Capabilities.h`) rather than in `tui`, because reading `COLORTERM`
      is I/O and `cli` may not see `tui`; `tui` may see `infra`, so Phase 4 consumes it.
- [ ] No compile-time terminal branching remains. Still Phase 4's: nothing renders yet.
- [ ] **Decide**: this issue says `NO_COLOR` takes effect "even empty", but the `Environment`
      port treats empty as unset throughout, and no-color.org itself says `NO_COLOR` applies
      "when present and not an empty string". TI-076 implemented the latter and pinned it with
      a test. Confirm or change it here, deliberately.

---

## TI-083 — `Theme` and `ThemeLoader`

**Type** feat · **Size** M · **Priority** P1 · **Depends on** TI-082 · **Docs** [UX §4](../UX.md#4-theming)

Semantic colour set loaded from TOML; built-in themes plus user themes from the config
directory.

**Unit tests** (`ThemeLoaderTest.cpp`)
- Every built-in theme loads and defines every semantic colour.
- A user theme overrides a built-in of the same name.
- A missing colour key takes the default and warns rather than failing.
- An invalid hex value is rejected with the key named.
- An unknown extra key is ignored (forward compatibility).
- A missing theme falls back to the default and warns.
- `theme_version` handling per [VERSIONING §5](../VERSIONING.md#5-independent-version-numbers).

**Acceptance**
- [x] `typeit-dark`, `typeit-light`, `high-contrast`, and `mono` all ship in `assets/themes/`
      and all load — checked against the files themselves, so a theme that stops parsing fails
      the build rather than the first person to select it. Each is asserted to define **every**
      semantic colour, because a theme that omitted half of them would load, take the defaults,
      and quietly look like the default theme.
- [x] Colours are semantic (`text_incorrect`), never positional. The enum is the vocabulary and
      the key table is the only spelling, so the loader and any future settings screen cannot
      disagree about a name.
- [x] Landed in `app` + `infra`, not `tui`, which ARCHITECTURE §4.4's directory sketch shows.
      `tui` may not link toml++, so the parser has to be `infra` — the same split the
      configuration already has (`core::Config` is the value, `TomlConfigStore` is the parser).
      The value type sits in `app`, where both layers can see it.
- [x] A theme with a mistake in it still loads: a missing colour, a malformed hex value, an
      unknown key and a `theme_version` from the future are all warnings against the default.
      The one fatal case is a file that is not TOML at all — the one case where continuing
      would mean rendering a theme whose author cannot see the effect of what they wrote.

---

## TI-084 — `ColorQuantizer`

**Type** feat · **Size** M · **Priority** P1 · **Depends on** TI-083 · **Docs** [UX §4](../UX.md#4-theming)

Truecolor → 256 → 16 → monochrome downgrade, so a theme author writes one file and it works
everywhere. The current code hardcodes 256-palette entries (`Color::Grey82`, `Color::Salmon1`)
with no fallback at all.

**Unit tests** (`ColorQuantizerTest.cpp`)
- Known truecolor values map to the expected xterm-256 indices (table-driven against a
  reference table).
- 256 → 16 mapping preserves perceptual ordering (a darker colour never maps lighter).
- `Mono` maps every colour to an attribute (bold/reverse/underline), never to a colour.
- Quantisation is deterministic and idempotent at each level.
- **Property: distinct semantic colours in a theme remain distinguishable after quantisation to
  16 colours** — for every shipped theme. If two states collapse to the same colour, the theme
  is broken at that depth and the test says so.

**Acceptance**
- [x] Every shipped theme passes the distinguishability property — asserted over the theme
      *files*, not a copy of their values, at truecolor, 256 and 16.
- [x] **The property is stated over the colours a typing state is read from**, not all
      fourteen. `background`, `surface` and `border` are three shades of one dark on purpose,
      and demanding they stay distinct in a sixteen-colour palette would force every theme to
      be garish. What must survive is what the typist reads meaning from.
- [x] `mono` is excluded from the colour property by name, because it has no colour at all —
      that is its whole point. Its states are told apart by attributes instead, and there is a
      separate test that all four remain distinct that way.
- [x] **The property found a real defect and the fix is the interesting part.** Nearest-RGB
      distance — the obvious implementation — maps a pastel pink nearer to white than to red,
      so the default theme collapsed into two shades of white and `text_correct`,
      `text_corrected`, `text_incorrect` and `caret` became two colours between them. The
      quantiser now asks what colour something *is* (hue) and then how bright, which is what
      keeps a pastel palette readable at sixteen colours.

---

## TI-085 — `GlyphSet`

**Type** feat · **Size** S · **Priority** P1 · **Depends on** TI-082 · **Docs** [UX §5](../UX.md#5-glyph-sets)

Unicode and ASCII variants of every decorative character.

**Unit tests** (`GlyphSetTest.cpp`)
- Both sets define every glyph in the UX §5 table (table-driven over the enum — a new glyph
  without an ASCII form fails the test).
- **The ASCII set contains no byte above 0x7F.** Explicit assertion.
- Selection follows capabilities and the config override.

---

## TI-086 — `Keymap` and binding parser

**Type** feat · **Size** M · **Priority** P1 · **Depends on** TI-079 · **Docs** [GAMEPLAY §9](../GAMEPLAY.md#9-default-keybindings)

Every action rebindable, with validation at config load.

**Unit tests** (`KeymapTest.cpp`, `KeyBindingParserTest.cpp`)
- Parse `ctrl-q`, `escape`, `f1`, `alt-x`, `?`, `ctrl-,` and their canonical round-trips.
- Case-insensitive and whitespace-tolerant parsing.
- An unparseable binding is rejected with the offending string named.
- A duplicate binding for two actions is rejected, naming both.
- An unbound action falls back to its default and warns.
- Bindings a terminal cannot deliver are flagged (not silently ignored) for `--doctor`.
- **`Ctrl+T` is not a default anywhere** — it is `SIGINFO` on BSD, a tab key in several
  emulators, and collides with common tmux prefixes. Explicit test guarding the regression.

---

## TI-087 — `TypingArea` widget

**Type** feat · **Size** L · **Priority** P0 · **Depends on** TI-081, TI-083, TI-085 · **Docs** [ADR-011](../ARCHITECTURE.md#adr-011--do-not-use-ftxuiinput-consume-key-events-directly), [TECHNICAL §3.1](../TECHNICAL.md#31-typingarea-adr-011)

The centrepiece. A custom `ComponentBase` that handles key events itself and renders from the
model. **No `ftxui::Input`, no bound `std::string`** — the current design infers "the character
just typed" from `input_text_.back()`, which is unsound for multi-byte input and paste, and is
the actual source of the ćčšđž problem the README attributes to FTXUI.

**Scope**
- In: `OnEvent` mapping `Event::Character` / `Event::Backspace` to model calls; `Render`
  drawing target text with per-state colouring, the caret, and (later) the pacer marker;
  re-wrapping on column change.
- Out: race-specific rendering (TI-125).

**Unit tests** (`TypingAreaTest.cpp`) — snapshot and interaction
- Correct/incorrect/pending/corrected states render with the right attributes.
- An incorrect space renders as `_` (behaviour worth keeping from 1.0.0).
- The caret lands in the correct cell **after wide characters** and **after combining marks**.
- Multi-byte input via `Event::Character("č")` produces exactly one grapheme in the model —
  the direct regression test for the README's ćčšđž note.
- Paste (a multi-grapheme `Event::Character`) is handled or rejected per the documented
  decision, but never desynchronises the model.
- Backspace at position 0 is a no-op and does not throw.
- Resize from 80 to 60 to 120 columns re-wraps and **preserves cursor and model state
  exactly**.
- Snapshots at 80×24, 120×40, and 60×20.
- **`Render()` called twice produces identical output and leaves the model byte-identical.**
  This is the standing regression guard against
  [the render-side-effect pattern](../CODEBASE_REVIEW.md#32-the-model-updates-itself-while-rendering)
  and it is the single most important test in this phase.
- `blind_mode` hides state colouring until the run ends.

**Acceptance**
- [x] `git grep "ftxui::Input"` returns nothing in `tui`. The widget consumes `Event::Character`
      directly, which is the whole of ADR-011.
- [x] The render-purity test passes — and asserts both halves: the same pixels twice, and the
      model byte-identical afterwards (states, cursor and log length). A widget that redrew
      correctly while advancing the log would pass a pixel comparison alone.
- [x] The multi-byte test passes, closing the README's documented limitation. It is not FTXUI:
      `Event::Character` has always carried the whole UTF-8 sequence, and 1.0 reads back
      `input_text_.back()` — one byte of it. `ćčšđž` all work, one keystroke each.
- [x] A paste (one event, several graphemes) is **typed in turn** rather than dropped. Losing
      all but the first would leave the model disagreeing with what the screen shows, which is
      worse than either accepting or refusing it.
- [x] The widget drives `core::Session`, not `TypingModel` directly, so the mode hears about
      every keystroke exactly as it does in a scripted run — one code path, not two.
      TECHNICAL §3.1's sketch predates `Session`.

---

## TI-088 — `StatsBar` and `KeyHintBar`

**Type** feat · **Size** S · **Priority** P1 · **Depends on** TI-083 · **Docs** [UX §3.2](../UX.md#32-session--timed)

**Unit tests** (`StatsBarTest.cpp`)
- **Fixed-width fields**: a WPM changing from 9 to 100 does not change the bar's width and
  therefore never reflows the text under the user's fingers.
- Values format correctly at boundaries (0, 999, 100.0%).
- Hidden metrics per config are actually absent, and the layout still balances.
- The hint bar shows context-appropriate bindings and reflects rebinds.

---

## TI-089 — Responsive layout and resize

**Type** feat · **Size** M · **Priority** P0 · **Depends on** TI-087 · **Docs** [UX §6.4](../UX.md#64-resize)

Layout computed from `Terminal::Size()`. No hardcoded `WIDTH EQUAL 75`, `HEIGHT EQUAL 10`, or
55-column wrap — the current fixed sizes make the UI unusable below ~80 columns and ignore
resize entirely.

**Unit tests** (`LayoutTest.cpp`)
- Layout adapts at 80×24, 100×30, 120×40, 200×50.
- `line_width = 0` fits the terminal; a fixed value is honoured and centred.
- `compact` and `comfortable` densities differ as documented.
- Resize mid-session preserves model state (asserted on the model, not just the render).
- A rapid sequence of resizes does not corrupt state.

---

## TI-090 — `TerminalTooSmallScreen`

**Type** feat · **Size** XS · **Priority** P1 · **Depends on** TI-089

**Unit tests** (`TerminalTooSmallTest.cpp`)
- Appears below 80×24, disappears at or above it, with hysteresis so a terminal sitting exactly
  on the boundary does not flicker.
- Reports the current and required size.
- Quit still works from it.
- **Session state survives** shrinking below the threshold and growing back.

---

## TI-091 — `MenuScreen`

**Type** feat · **Size** M · **Priority** P0 · **Depends on** TI-081, TI-086 · **Docs** [UX §3.1](../UX.md#31-menu)

Mode, duration/word-count, and text selection, plus the recent-runs sparkline.

**Unit tests** (`MenuScreenTest.cpp`)
- Selection state is held by the screen, not by a global.
- Custom duration input validates and reports the range on rejection.
- **Custom input validation happens on change, not inside a render transform** — the current
  code runs `std::stoi` in a `try`/`catch` with an **empty catch block** inside a render
  callback, on every frame.
- Starting with an invalid selection is refused with a visible message rather than silently
  doing nothing.
- Keyboard navigation reaches every control; tab order is stable.
- Snapshot at 80×24.

**Acceptance**
- [x] Mode, duration and word count are held by the screen and validated on change.
- [x] Text selection: the three bundled corpora, and one position past the last for a path the
      user types. Landed after the rest of the screen, because nothing wired the menu to the
      application until TI-098 and the gap was invisible until something ran.
- [x] The path is validated by *opening* it, not by asking whether it exists — "it is there"
      and "I can read it" are different answers and the run needs the second. The menu never
      touches a disk itself: `ScreenContext::load_text` is supplied by the composition root,
      which is the only layer allowed to.
- [x] With no catalogue found the field reads `built-in` and cannot be edited, rather than
      offering three entries that fail the moment one is chosen.

---

## TI-092 — `SessionScreen`

**Type** feat · **Size** M · **Priority** P0 · **Depends on** TI-087, TI-088, TI-068

Wires `TypingArea`, `StatsBar`, and the mode into a running session.

**Unit tests** (`SessionScreenTest.cpp`)
- Tick events advance the mode; **key events advance the model**; neither happens in `Render`.
- The countdown, when enabled, delays the start and the timer still begins on the first
  keystroke.
- Mode completion pushes `ResultsScreen` exactly once.
- Restart produces a genuinely fresh session — no state carried over. Regression guard for the
  globals.
- Quitting mid-run records an abandoned session.
- Confirm-on-quit is honoured when configured.

---

## TI-093 — `ResultsScreen` (parity version)

**Type** feat · **Size** S · **Priority** P0 · **Depends on** TI-092

Headline numbers and the four actions. Charts arrive in Phase 5.

**Unit tests** (`ResultsScreenTest.cpp`)
- All metrics render with correct formatting and units.
- A zero-keystroke session renders without NaN, empty fields, or a crash.
- Each action navigates correctly (restart, new text, menu).
- Snapshot at 80×24.

**Acceptance**
- [x] It renders the `SessionRecord` that was written to the database rather than recomputing
      from the session. A results screen that recomputed would be a second implementation of
      the metrics, and the two would disagree eventually.
- [x] Numbers go through the same `app::json::number` the exports use, so a figure on screen
      and the same figure in a CSV cannot disagree.
- [x] The screen navigates nothing itself — it reports what was asked for and whoever drives
      the stack acts on it. A screen that pushed would have to know about every other screen.

---

## TI-094 — `HelpScreen`

**Type** feat · **Size** XS · **Priority** P2 · **Depends on** TI-086

**Unit tests**
- Lists **actual current bindings**, including rebinds — not a hardcoded list that drifts.
- Includes the one-line note that font size is a terminal setting
  ([UX §1](../UX.md#1-what-is-and-is-not-customisable)).
- Scrolls when content exceeds the terminal height.

---

## TI-095 — Render snapshot harness

**Type** test · **Size** M · **Priority** P0 · **Depends on** TI-079 · **Docs** [TESTING §6](../TESTING.md#6-render-tests--typeittui)

Render a component to a fixed-size `ftxui::Screen`, compare against a golden text file. FTXUI
supports this and the current project uses none of it.

**Scope**
- In: the harness; golden file storage and update workflow (`TYPEIT_UPDATE_GOLDENS=1`); a
  readable diff on mismatch; the reusable "render twice, model unchanged" assertion.
- Out: individual snapshots (they live with their widgets).

**Acceptance**
- [x] Goldens are plain text and reviewable as code in a diff — **styling stripped**. A golden
      full of `\x1b[38;2;205;214;244m` is technically plain text and reviewable by nobody. What
      a snapshot is for is layout; colour has its own tests, and `render_to_styled_text` is
      there for the two that are about it.
- [x] Updating a golden is deliberate and visible: `TYPEIT_UPDATE_GOLDENS=1` rewrites them, CI
      never sets it, and a run that rewrote a golden **skips rather than passes** — so nobody
      commits an "all green" that only means the files now agree with whatever the code does.
- [x] The purity assertion is available to every widget test as
      `testing::expect_render_is_pure`. The *model* half cannot be generic — the harness does
      not know what state a widget draws from — so a widget that owns one asserts on it
      directly, as `TypingAreaTest` does.

---

## TI-096 — Windows console initialisation

**Type** feat · **Size** M · **Priority** P0 · **Depends on** TI-079 · **Docs** [UX §6.3](../UX.md#63-windows-specifics)

Enable VT processing, set UTF-8 code pages, embed a UTF-8 manifest, and **restore the original
console state on exit** — including on abnormal exit.

**Unit tests** (`WindowsConsoleTest.cpp`, Windows-only)
- Original console mode and code page are captured and restored.
- Restoration happens on normal exit, on exception, and on Ctrl+C.
- Enabling VT on a console that does not support it fails gracefully rather than aborting.
- Non-ASCII output renders correctly (verified via the snapshot harness).

**Acceptance**
- [x] Verified by the Windows CI job, not assumed — the three Windows configurations build and
      run the suite, which is what "checked" means here.
- [x] The original mode and both code pages are captured **before** anything is changed, so the
      destructor puts back what was really there rather than what the code assumes. RAII, so it
      happens on a normal return and on an exception unwinding through `main`.
- [x] A console too old for VT processing gets the ASCII glyph set and a note for `--doctor`,
      not an aborted start.
- [x] The type exists on every platform, so the composition root has no `#ifdef` in it — a
      platform test at the call site is how a platform bug hides. It is **no longer empty off
      Windows**, and the rename to `ConsoleMode` says so: a Unix tty in its default state
      intercepts `Ctrl+S` and `Ctrl+Q` as XOFF and XON and never delivers them, so the shipped
      force-quit binding could not be pressed at all. FTXUI clears `ICANON` and `ECHO` and
      nothing else. `IXON` and `IXANY` are cleared on the way in, and only `c_iflag` is put
      back on the way out — restoring a whole `termios` captured before FTXUI ran would undo
      FTXUI's raw mode as a side effect. Found by TI-098, which is the first time anything ran
      the program.
- [ ] **Ctrl+C is not covered.** It does not run destructors, so restoring on it needs a
      console control handler, and there is no Windows machine here to check one on. Left for
      the Windows CI job to prove or disprove rather than written blind.

---

## TI-097 — Cutover: delete the legacy tree

**Type** refactor · **Size** M · **Priority** P0 · **Depends on** TI-098 · **Docs** [ROADMAP Phase 4](../ROADMAP.md#phase-4--typeittui-and-cutover)

One commit, reviewable as a whole. Only after TI-098 passes.

**Scope**
- In: delete `include/`, `src/core/`, `src/view/`, `src/engines/`, `src/main.cpp`, the legacy
  `tests/test_*.cpp`, and the `typeit_legacy` target; move `files/*.txt` to `assets/texts/`;
  remove the legacy clang-tidy exclusion added in TI-014.
- Out: any behavioural change — this commit deletes only.

**Acceptance**
- [ ] `git grep -l "GameState\|GameOptions\|FocusPosition"` returns nothing.
- [ ] `git grep -l "ftxui::Input"` returns nothing.
- [ ] `git grep __FILE__` returns nothing outside diagnostics.
- [ ] No relative `../../include/` include remains.
- [ ] The build has no `typeit_legacy` target; the clang-tidy exclusion is gone.
- [ ] Full suite green on the entire CI matrix.

---

## TI-098 — Parity verification

**Type** test · **Size** M · **Priority** P0 · **Depends on** TI-091 – TI-096

The written checklist that gates TI-097. Everything 1.0.0 does, 2.0.0-alpha.5 must do.

**Parity checklist**

Each item below is ticked on two grounds: a named automated test, and one end-to-end run of the
real binary driven through a pty on Linux. That is **not** the four-terminal sweep the
acceptance asks for — see the acceptance note.

- [x] Three bundled difficulty texts selectable —
      `MenuScreenTest.TheBundledTextsCycleWithTheArrowsAndEndAtACustomPath`; the corpora moved
      from `files/` to `assets/texts/` and are found through the asset search path.
- [x] Custom file path input with validity feedback —
      `MenuScreenTest.ACustomPathThatCannotBeReadIsReportedAndRefusesToStart` and
      `…ThatReadsClearsTheMessageAndStarts`.
- [x] 15 / 30 / 60 second and custom timers — any duration in range is typed into the field.
      This was broken until it was run: the registry's factories took no arguments, so the
      duration was baked in at registration and choosing fifteen seconds changed the number
      written to the history and nothing about the run.
      `ModeRegistryTest.TheParameterReachesTheFactoryThatWasRegisteredForIt` is the guard.
- [x] Live WPM display — `StatsBarTest`, and read off a live run.
- [x] Live accuracy display — `StatsBarTest`.
- [x] Countdown of remaining time — `StatsBarTest`; a live 15-second run ended at fifteen
      seconds and produced a saved record.
- [x] Per-character colouring: correct, incorrect, untyped — `TypingAreaTest.TheFourStatesRenderDistinctly`.
- [x] Incorrect space rendered as `_` — `TypingAreaTest.AnIncorrectSpaceRendersAsAnUnderscore`.
- [x] Multi-line text with automatic scrolling as you type —
      `TypingAreaTest.TheViewScrollsToFollowTheCursorOffTheBottom`.
- [x] Backspace including across a line boundary — `TypingAreaTest.BackspaceCrossesALineBoundary`.
- [x] Restart the current session from a key — `Ctrl+R`; the restart reads the selection back
      off the menu underneath rather than a default one.
- [x] Return to menu from a key — `Escape`, and the run in progress is saved as abandoned
      rather than dropped.
- [x] Session-complete state shown when the text is finished or time expires — `ResultsScreen`,
      seen at the end of the live 15-second run.
- [x] Info/help text available from the menu — `F1`, `HelpScreenTest.ItListsTheActualBindings`;
      the live run showed the real bindings.

**Improvements delivered at the same time** (verified, not merely claimed)
- [x] Metrics are correct (C4, C5 closed) — `SessionServiceTest.TheHeadlineMetricsComeFromTheLogRatherThanTheConfiguredDuration`
      and `…AnUncorrectedMistakeIsCountedTwice`.
- [x] Terminal resize supported mid-session — `TypingAreaTest.ResizePreservesCursorAndModelExactly`,
      `…ARapidSequenceOfResizesDoesNotCorruptState`, `LayoutTest`, and the too-small gate's
      hysteresis test. The size is re-read every frame rather than cached.
- [x] Themes and colour-depth fallback — `ThemeLoaderTest`, `ColorQuantizerTest`.
- [x] ASCII glyph fallback — `GlyphSetTest`.
- [x] Rebindable keys, with `Ctrl+T` retired — `KeymapTest`;
      `HelpScreenTest.ItFollowsARebindRatherThanAHardcodedList`. `git grep` for `Ctrl+T` in the
      rebuilt tree returns nothing.
- [x] Every run recorded to history — `SessionServiceTest.EverythingIsWrittenThroughOneCall`;
      confirmed against the real database after a live run, including an abandoned one.
- [x] Binary is relocatable (C2 closed) — `AssetLocatorTest`; no `__FILE__` in the rebuilt tree.
- [x] No global mutable state — `TerminalAppTest.TwoApplicationsDoNotShareState`,
      `MenuScreenTest.TheSelectionIsHeldByTheScreen`, and the allocation-counter suite.

**Defects this verification found** (all fixed here, each with a regression test)
- The chosen duration and word count never reached the mode — see the timers item above.
- `Ctrl+Q` could not be pressed at all. A tty in its default state intercepts `Ctrl+S` and
  `Ctrl+Q` as XOFF and XON; FTXUI clears `ICANON` and `ECHO` and nothing else, so the shipped
  force-quit binding never arrived and the application had to be killed from another terminal.
  `ConsoleMode` (was `WindowsConsole`) now clears `IXON` and `IXANY` on the way in and restores
  the input flags on the way out.
- The results screen truncated its own numbers. `json::number` gives six decimals and the field
  is eight wide and truncates from the *left*, so a 27 WPM run was displayed as `7.972028`.
  Figures are rounded for reading; the export keeps its precision.
- Restarting from the results screen built a default `MenuSelection`, so a sixty-second run
  restarted as a thirty-second one.

**Acceptance**
- [ ] Every parity item verified **manually** in alacritty, kitty, GNOME Terminal, and Windows
      Terminal, at 80×24 and 120×40, with the result recorded in the issue.
      **Outstanding, and not something an automated run can close.** What has been done is the
      list above: every item has a test, and the whole flow — menu, text choice, countdown,
      typing, completion, results, history — was driven end to end through a pty on Linux at
      80×24. What that cannot see is what these four terminals actually put on a screen:
      glyph widths, colour rendition at each depth, and whether a resize mid-run redraws
      cleanly. Someone has to look. Until they have, this box stays empty and **TI-097 does not
      start** — its own scope says "only after TI-098 passes".
- [x] Every improvement item has an automated test — named beside each item above.

---

## Phase exit criteria

- [x] TI-098 checklist fully ticked — the parity and improvement lists are. Its **acceptance**
      is not: the four-terminal manual sweep is outstanding, which is what still holds TI-097.
- [ ] Legacy tree deleted; the grep checks in TI-097 are clean. Blocked on that sweep, by
      TI-097's own scope. The one part done early is the corpora: `files/*.txt` moved to
      `assets/texts/` because the menu needed somewhere to find them.
- [x] `tui` coverage ≥ 60%; every widget has at least one snapshot and one interaction test.
      It measures **84.54%** (1154/1365 lines), and the gate is now in `scripts/coverage.sh`
      rather than in this document — a criterion nothing checks is a wish.
- [x] The render-purity assertion is applied to every widget.
      `ScreenPurityTest.NoScreenChangesAnythingByDrawing` covers the menu, help, too-small and
      results screens; `SessionScreenTest.RenderingAdvancesNeither` and
      `TypingAreaTest.RenderTwiceChangesNothing` cover the two that own a model, and assert the
      model as well as the pixels.
- [x] Full suite green on all CI configurations, including Windows — first observed whole at
      `37836da`. It had never been true: the Windows jobs failed at build, then at test, and
      the release configuration had never run its tests at all, which is where the
      `EXPECT_DEBUG_DEATH` problem was hiding.
- [x] `CHANGELOG.md` records the breaking changes: metric definitions, keybindings, data
      locations.
