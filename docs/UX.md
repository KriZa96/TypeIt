# TypeIt — Interface, Theming, and Terminal Compatibility

*Screen designs, what is customisable, and the honest boundaries of what a terminal
application can control.*

---

## 1. What is and is not customisable

Being straight about this up front, because it shapes everything below.

### Not customisable — and TypeIt will not pretend otherwise

**Font family and font size.** A terminal application draws characters into a grid; the
terminal emulator decides what typeface renders that grid and at what point size. There is no
escape sequence, no library call, and no FTXUI feature that changes it. Any application that
appears to offer this is either a GUI application or is lying.

TypeIt therefore ships **no font settings**, no fake "text size" slider, and no GUI frontend
added solely to acquire the capability. If you want bigger text while typing, that is
`Ctrl` + `+` in most terminals — one line in the help screen, and nothing more.

### Customisable — and worth doing well

| Area | Options |
|---|---|
| **Colour theme** | Built-in themes plus user themes as `.toml` files; every semantic colour independently settable |
| **Colour depth** | truecolor / 256 / 16 / monochrome, auto-detected and overridable; `NO_COLOR` respected |
| **Glyph set** | Unicode or pure ASCII for borders, carets, markers, and progress bars |
| **Caret** | block / underline / outline / none, blinking or steady |
| **Layout** | compact or comfortable density; visible line count; text column width or fit-to-terminal |
| **HUD** | Which live metrics are shown, and whether they are shown at all |
| **Typing rules** | Error blocking, backspace policy, strict spaces, blind mode, confidence mode |
| **Keybindings** | Every action rebindable |
| **Text pipeline** | Typographic flattening, whitespace collapsing, punctuation/case stripping, tab width, chunk size |
| **Race difficulty** | Every parameter of the ramp law |

That is a deep customisation surface. It just does not include the two things the terminal
owns.

---

## 2. Screen map

```
                    ┌──────────────┐
                    │  MenuScreen  │◄──────────────┐
                    └──────┬───────┘               │
       ┌───────────┬───────┼────────┬──────────┐   │
       ▼           ▼       ▼        ▼          ▼   │
  ┌─────────┐ ┌────────┐ ┌──────┐ ┌────────┐ ┌─────────┐
  │ Session │ │History │ │Library│ │Settings│ │  Help   │
  └────┬────┘ └───┬────┘ └──────┘ └────────┘ └─────────┘
       │          │
       ▼          ▼
  ┌─────────┐ ┌──────────────┐
  │ Results │ │ SessionDetail│
  └────┬────┘ └──────────────┘
       │
       └── restart │ new text │ drill │ menu
```

Navigation is a `ScreenStack` ([TECHNICAL §3.2](TECHNICAL.md#32-screen-stack)), replacing the
five interacting booleans in the current `GameState`.

---

## 3. Screen designs

### 3.1 Menu

```
┌─ TypeIt ─────────────────────────────────────────────────────────────┐
│                                                                      │
│   Mode      ● timed   ○ words   ○ quote   ○ endless   ○ race        │
│   Duration  ○ 15s     ● 30s     ○ 60s     ○ 120s      ○ custom      │
│   Text      ● random  ○ library ○ file…                             │
│                                                                      │
│   ┌────────────────────────────────────────────────────────────┐    │
│   │  Last 10 runs                                              │    │
│   │  ▁▂▃▅▄▆▇▆█▇   avg 68 wpm   best 81 wpm   acc 96.2%        │    │
│   └────────────────────────────────────────────────────────────┘    │
│                                                                      │
│                    [ Start ]   [ History ]   [ Quit ]               │
│                                                                      │
│  ^L library   ^H history   F2 settings   F1 help                    │
└──────────────────────────────────────────────────────────────────────┘
```

The sparkline of recent runs sits on the menu deliberately: the whole point of recording
history is to see it without asking.

**What 2.0.0-alpha.5 actually draws**, since the sketch above is the destination rather than a
screenshot: one row per control — mode, seconds, words, text, start — with left and right
cycling the list-valued ones. The text row offers the three bundled corpora and, one position
past the last, a path the user types, which is validated by opening it. The sparkline is not
there: it reads history, and the query behind it is Phase 5. `endless` and `race` are not there
either, for the same reason — they are Phase 7.

### 3.2 Session — timed

```
┌──────────────────────────────────────────────────────────────────────┐
│  00:23          68 wpm          97.4%          ████████░░░░  62%    │
├──────────────────────────────────────────────────────────────────────┤
│                                                                      │
│   the quick brown fox jumps over the lazy dog and then keeps         │
│   ▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔█                                │
│   running until it reaches the far side of the quiet field           │
│   where nothing at all is waiting for it                             │
│                                                                      │
├──────────────────────────────────────────────────────────────────────┤
│  esc menu    ^R restart    ^N new text                              │
└──────────────────────────────────────────────────────────────────────┘
```

Correct graphemes render in the theme's `text_correct`, untyped in `text_pending`, incorrect in
`text_incorrect` (with an incorrect space shown as `_`, a behaviour worth keeping from the
current implementation), and corrected in `text_corrected`. The caret sits between graphemes.

Layout is **computed from the terminal size**, never hardcoded. The current implementation
pins width 75, height 10, and wraps text at 55 columns, which is unusable below ~80 columns
and ignores resize entirely. Here, `wrap()` is re-run whenever the column count changes and the
session continues uninterrupted — possible only because wrapping is a pure function of the
domain text rather than state baked into it (ADR-001).

### 3.3 Session — race

```
┌──────────────────────────────────────────────────────────────────────┐
│  target 74 wpm ▲      you 81 wpm      98.1%      lead 31      ♥♥    │
├──────────────────────────────────────────────────────────────────────┤
│  pacer  ├────────────▓▓▓▓░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░┤       │
├──────────────────────────────────────────────────────────────────────┤
│                                                                      │
│   consider the ordinary afternoon when nothing much happens          │
│   ▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔█                            │
│   and the light moves slowly across the floor of the room            │
│                                                                      │
└──────────────────────────────────────────────────────────────────────┘
```

The pacer bar is the primary feedback channel: the filled section is the pacer, the caret marks
the player, and the gap between them *is* the lead. `▲`/`▼`/`=` next to the target speed shows
whether the ramp is currently climbing, backing off, or in the dead band — so the player can
feel the accuracy gate (`g(A)` in [GAMEPLAY §3.2](GAMEPLAY.md#32-the-ramp-law)) working rather
than wonder why the speed stopped rising.

The pacer is also rendered inline in the text as a coloured column, so the threat is visible
where the eyes already are.

### 3.4 Results

```
┌─ Session complete ───────────────────────────────────────────────────┐
│                                                                      │
│      71 wpm net          75 gross          79 raw                   │
│      96.8% accuracy      98.9% correct     84 consistency           │
│                                                                      │
│   wpm  ┤                                        ▁▂▃                 │
│    90  ┤                          ▁▃▄▅▆▇█▇▆▅▄▃▂▁                    │
│    60  ┤        ▁▂▄▆▇█▇▆▄▂▁                           ×    ×        │
│    30  ┤▁▃▅▇█                                                       │
│         └────────────────────────────────────────────────────       │
│          0s        10s       20s       30s       40s      50s       │
│                                                                      │
│   Personal best 81 wpm · you were 10 below · avg over 30d: 68       │
│                                                                      │
│   Worst pairs      t→r  ×7    ie→ei ×5    ;→l  ×4                   │
│   Slowest bigrams  "br" 210ms   "wh" 195ms   "ck" 180ms             │
│                                                                      │
│   [ Restart ]  [ New text ]  [ Drill weak keys ]  [ Menu ]          │
└──────────────────────────────────────────────────────────────────────┘
```

`×` marks on the chart are errors, positioned in time — which makes "I fall apart after 30
seconds" visible rather than a hunch. After a race, this screen additionally names the speed
wall and the pairs that failed inside it
([GAMEPLAY §3.6](GAMEPLAY.md#36-post-race-analysis--the-speed-wall)).

### 3.5 History

```
┌─ History ────────────────────────── mode: all ▾   range: 30 days ▾ ──┐
│                                                                      │
│   wpm  ┤                                              ▂▄▆█▇         │
│    80  ┤                            ▁▃▄▆▇█▇▆▅▄▃                     │
│    60  ┤      ▁▂▃▄▅▆▇█▇▆▅                                           │
│    40  ┤▁▂▃▄▅                                                       │
│         └──────────────────────────────────────────────────         │
│          Jul 1                Jul 15                 Jul 30         │
│                                                                      │
│   142 runs · 3h 21m typed · streak 12 days · longest 19             │
│                                                                      │
│   Personal bests    15s 84 · 30s 81 · 60s 76 · race peak 92         │
│                                                                      │
│   Error heatmap                                                      │
│     q w e r t y u i o p     ░ = clean                                │
│      a s d f g h j k l      ▒ = occasional                           │
│       z x c v b n m         █ = frequent                             │
│                                                                      │
│   Recent                                                             │
│     Jul 30 19:04  timed 30s   71 wpm  96.8%   simple.txt            │
│     Jul 30 18:52  race        peak 74  94.1%  rust-book             │
└──────────────────────────────────────────────────────────────────────┘
```

The keyboard heatmap is the payoff for recording per-key statistics: it turns thousands of
keystrokes into one glance that says which fingers to work on.

### 3.6 Text library

```
┌─ Texts ──────────────────────────────────── search: ______________ ──┐
│                                                                      │
│   ▸ The Rust Book — ch.4        1,204 words  diff 6.8  ██░░ 42%     │
│     Personal notes 2026         3,881 words  diff 4.2  ░░░░  0%     │
│     simple (builtin)              158 words  diff 2.1  ████ 100%    │
│                                                                      │
│   [ Import file ]  [ Paste ]  [ Tag ]  [ Delete ]                   │
└──────────────────────────────────────────────────────────────────────┘
```

The progress bar is the chunk bookmark from
[GAMEPLAY §5](GAMEPLAY.md#5-text-supply--type-anything-you-want): long documents are typed
across many sessions and remember where you were.

### 3.7 Terminal too small

Below the minimum usable size (80 × 24), every screen is replaced by:

```
   Terminal too small
   Need at least 80 x 24, currently 62 x 18.
   Resize, or press q to quit.
```

Better than the current behaviour, where fixed-size decorators silently produce a broken
layout.

---

## 4. Theming

Themes are `.toml` files declaring semantic colours, loaded from
`$XDG_CONFIG_HOME/typeit/themes/` (user) and the installed assets directory (built-in).

```toml
name        = "typeit-dark"
author      = "…"
description = "Default dark theme"

[colors]
background     = "#1e1e2e"
surface        = "#313244"
border         = "#45475a"
text_pending   = "#6c7086"
text_correct   = "#cdd6f4"
text_incorrect = "#f38ba8"
text_corrected = "#f9e2af"
caret          = "#89b4fa"
pacer          = "#a6e3a1"
accent         = "#89b4fa"
success        = "#a6e3a1"
warning        = "#f9e2af"
error          = "#f38ba8"
muted          = "#585b70"
```

Colours are **semantic, not positional** — `text_incorrect`, not `color_3` — so a theme author
never has to know where a colour is used, and adding a UI element does not invalidate every
theme.

Shipped themes: `typeit-dark`, `typeit-light`, `high-contrast`, `mono`, plus a couple of
familiar palettes. `mono` uses no colour at all and distinguishes state by underline, reverse
video, and bold, which is what makes TypeIt usable over a serial console or with `NO_COLOR`.

**Downgrade path.** Themes are authored in truecolor. `ColorQuantizer` maps to the xterm-256
cube and then to the 16 ANSI colours, and `mono` maps everything to attributes. A theme author
writes one file and it works everywhere — as opposed to the current code, which hardcodes
256-palette entries (`Color::Grey82`, `Color::Salmon1`) with no fallback at all.

---

## 5. Glyph sets

Every decorative character has a Unicode and an ASCII form, selected by capability detection or
by config.

| Element | Unicode | ASCII |
|---|---|---|
| Border | `│ ─ ┌ ┐ └ ┘` | `\| - + + + +` |
| Radio selected / empty | `◉` / `○` | `(*)` / `( )` |
| Caret block | `█` | `#` |
| Caret underline | `▁` | `_` |
| Progress filled / empty | `█` / `░` | `#` / `.` |
| Sparkline | `▁▂▃▄▅▆▇█` | `.:-=+*%@` |
| Trend up / down / flat | `▲ ▼ =` | `^ v =` |
| Lives | `♥` | `*` |

This replaces the `FTXUI_MICROSOFT_TERMINAL_FALLBACK` macro in the current code, which is
branched on but **never defined by any build file** — so the Unicode branch always compiles and
the fallback has never once executed.

---

## 6. Terminal compatibility

### 6.1 Support matrix

Tier 1 is verified before each release; tier 2 is expected to work and is fixed on report.

| Tier | Linux | Windows |
|---|---|---|
| **1** | alacritty, kitty, foot, GNOME Terminal, Konsole, xterm, tmux, WezTerm | Windows Terminal, WSL, PowerShell 7 in conhost |
| **2** | Linux virtual console (TTY), st, urxvt, screen, VS Code integrated terminal | cmd.exe, Git Bash / mintty, ConEmu, VS Code integrated terminal |

Known constraints, documented rather than papered over: the Linux TTY has 16 colours and no
box-drawing beyond CP437, so it gets `Ansi16` + `Ascii`. mintty does not deliver some `Ctrl`
combinations, so `--doctor` reports which bindings are unavailable there.

### 6.2 Capability detection

Order of precedence — config always wins over detection:

1. `NO_COLOR` set → `Mono`
2. `TYPEIT_COLOR` / `TYPEIT_GLYPHS` environment overrides
3. `COLORTERM` ∈ {`truecolor`, `24bit`} → `TrueColor`
4. `WT_SESSION` present → `TrueColor` + `Unicode` (Windows Terminal)
5. `TERM` contains `256color` → `Ansi256`
6. `TERM` ∈ {`linux`, `dumb`} → `Ansi16` + `Ascii`
7. Fallback → `Ansi16` + `Ascii`

The floor is deliberately conservative: an unknown terminal gets something that certainly
works rather than something that probably looks better.

### 6.3 Windows specifics

- Enable `ENABLE_VIRTUAL_TERMINAL_PROCESSING` on stdout, and restore the original console mode
  on exit.
- `SetConsoleOutputCP(CP_UTF8)` and `SetConsoleCP(CP_UTF8)`, restored on exit.
- A UTF-8 manifest embedded in the executable.
- `/utf-8` at compile time (see [BUILD.md §6](BUILD.md#6-warnings)).
- FTXUI performs most of this already; the point is that it is **verified by a Windows CI job**
  rather than assumed. There is currently no Windows CI at all.

### 6.4 Resize

`SIGWINCH` (and the Windows equivalent) is handled by FTXUI; TypeIt responds by recomputing
`wrap()` and the layout. **Session state is untouched** — the domain model stores graphemes,
not lines. Resizing mid-run is a supported operation, not a crash and not a restart.

### 6.5 Key delivery

Terminals cannot deliver every key combination, and which ones fail differs by emulator. So:

- Every binding is configurable.
- Defaults avoid the known-bad set — notably **`Ctrl+T` is not used**: it is `SIGINFO` on BSD,
  a tab-management key in several emulators, and collides with common tmux prefixes. The
  current application binds session exit to it.
- Bindings are validated at config load; unresolvable ones produce a clear warning.
- `typeit --doctor` prints detected capabilities, resolved paths, database health, and a live
  key-press tester so a user in an unfamiliar terminal can find out what their terminal
  actually sends.

---

## 7. Accessibility

- `NO_COLOR` honoured; `mono` theme distinguishes every state without colour.
- `high-contrast` theme meets WCAG AA contrast ratios against its background.
- State is never encoded by colour alone — incorrect graphemes are also underlined, and an
  incorrect space renders as `_`.
- Blinking is off by default and configurable.
- No flashing, no animation faster than the frame ticker.
- All functionality is keyboard-reachable; the mouse is optional everywhere.

---

## 8. Interaction principles

1. **The typing area is the interface.** Chrome shrinks in compact layout; the text never
   does.
2. **Nothing moves under the fingers.** The HUD is fixed-width so a changing WPM figure never
   reflows the text.
3. **One keystroke to restart.** The fastest path from "that run went badly" to "typing again"
   is a single key, because that is the loop the whole app exists to serve.
4. **Context-sensitive hints on screen.** A permanent one-line key hint bar; the full list on
   `F1`.
5. **Destructive actions confirm.** Deleting a text or clearing history asks. Quitting mid-run
   asks, configurably.
6. **Errors are visible and actionable.** A failure to read a file names the file and says
   what to do. Nothing is swallowed — which is a direct reaction to the empty `catch` block in
   the current code.
