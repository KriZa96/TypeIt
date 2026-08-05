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
- [ ] `tui` cannot see `infra`; the composition root is the only place both are visible.
- [ ] FTXUI is `PRIVATE`, so it does not leak to `tui`'s consumers.

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
- [ ] No `sleep_for` in the tests; the file runs in well under 100 ms.
- [ ] The ticker holds no reference to any model type.

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
- [ ] No boolean navigation flag exists anywhere in `tui`.

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
- [ ] `typeit-dark`, `typeit-light`, `high-contrast`, and `mono` all ship and load.
- [ ] Colours are semantic (`text_incorrect`), never positional.

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
- [ ] Every shipped theme passes the distinguishability property at all four depths.

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
- [ ] `git grep "ftxui::Input"` returns nothing in `tui`.
- [ ] The render-purity test passes.
- [ ] The multi-byte test passes, closing the README's documented limitation.

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
- [ ] Goldens are plain text and reviewable as code in a diff.
- [ ] Updating a golden is deliberate and visible in the diff, never automatic in CI.
- [ ] The purity assertion is available to every widget test.

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
- [ ] Verified by the Windows CI job, not assumed. FTXUI does most of this; the point is that
      it is now checked.

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
- [ ] Three bundled difficulty texts selectable
- [ ] Custom file path input with validity feedback
- [ ] 15 / 30 / 60 second and custom timers
- [ ] Live WPM display
- [ ] Live accuracy display
- [ ] Countdown of remaining time
- [ ] Per-character colouring: correct, incorrect, untyped
- [ ] Incorrect space rendered as `_`
- [ ] Multi-line text with automatic scrolling as you type
- [ ] Backspace including across a line boundary
- [ ] Restart the current session from a key
- [ ] Return to menu from a key
- [ ] Session-complete state shown when the text is finished or time expires
- [ ] Info/help text available from the menu

**Improvements delivered at the same time** (verified, not merely claimed)
- [ ] Metrics are correct (C4, C5 closed)
- [ ] Terminal resize supported mid-session
- [ ] Themes and colour-depth fallback
- [ ] ASCII glyph fallback
- [ ] Rebindable keys, with `Ctrl+T` retired
- [ ] Every run recorded to history
- [ ] Binary is relocatable (C2 closed)
- [ ] No global mutable state

**Acceptance**
- [ ] Every parity item verified **manually** in alacritty, kitty, GNOME Terminal, and Windows
      Terminal, at 80×24 and 120×40, with the result recorded in the issue.
- [ ] Every improvement item has an automated test.

---

## Phase exit criteria

- [ ] TI-098 checklist fully ticked.
- [ ] Legacy tree deleted; the grep checks in TI-097 are clean.
- [ ] `tui` coverage ≥ 60%; every widget has at least one snapshot and one interaction test.
- [ ] The render-purity assertion is applied to every widget.
- [ ] Full suite green on all CI configurations, including Windows.
- [ ] `CHANGELOG.md` records the breaking changes: metric definitions, keybindings, data
      locations.
