# TypeIt — Architecture

*Target architecture for the rebuilt application. See [CODEBASE_REVIEW.md](CODEBASE_REVIEW.md)
for the problems this design exists to solve, and [ROADMAP.md](ROADMAP.md) for the order in
which it gets built. Everything here is drawn in [DIAGRAMS.md](DIAGRAMS.md).*

---

## 1. Goals and constraints

**Functional goals**

- Type any text the user supplies: files, stdin, pasted content, or the bundled corpora.
- Persist a full history of runs and derive meaningful metrics and trends from it.
- Deep customisation of everything a terminal application can actually control.
- An endless mode and a progressive **Race** mode that accelerates with the typist and is
  designed to raise their ceiling over weeks, not to punish them in one session.

**Non-functional goals**

- The domain is testable with zero terminal, zero database, and zero clock.
- Cross-platform: Linux and Windows, across a documented matrix of terminal emulators.
- A build that fails loudly on anything the compiler can detect.
- Illegal dependencies should be *impossible to write*, not merely discouraged.

**Explicit non-goals**

- **Font family and font size are out of scope.** A terminal application cannot set them;
  they belong to the terminal emulator. TypeIt will not pretend otherwise, will not ship a
  fake "font size" setting, and will not grow a GUI frontend to gain that capability. What
  *is* customisable is enumerated in [UX.md](UX.md).
- Networking, accounts, cloud sync, multiplayer.
- Full UAX #29 grapheme segmentation (a documented common subset is the target — see §6.2).

---

## 2. Style: Ports and Adapters

The application is layered as a hexagon. Dependencies point **inward only**. The domain
defines interfaces (*ports*); the outside world provides implementations (*adapters*).

```
                    ┌───────────────────────────────────┐
                    │        apps/typeit (main)         │
                    │       composition root only       │
                    └───────────────┬───────────────────┘
                                    │ constructs & injects
              ┌─────────────────────┼─────────────────────┐
              ▼                     ▼                     ▼
      ┌───────────────┐     ┌───────────────┐     ┌───────────────┐
      │ typeit::tui   │     │ typeit::infra │     │ typeit::cli   │
      │ (FTXUI)       │     │ (SQLite,TOML, │     │ (arg parsing) │
      │  driving      │     │  filesystem)  │     │   driving     │
      │  adapter      │     │ driven adapter│     │   adapter     │
      └───────┬───────┘     └───────┬───────┘     └───────┬───────┘
              │                     │                     │
              └──────────┬──────────┴──────────┬──────────┘
                         ▼                     ▲
                 ┌───────────────────────────────────┐
                 │          typeit::app              │
                 │  use cases / orchestration        │
                 │  defines driven ports             │
                 └───────────────┬───────────────────┘
                                 ▼
                 ┌───────────────────────────────────┐
                 │          typeit::core             │
                 │  domain model, pure C++           │
                 │  ZERO third-party dependencies    │
                 └───────────────────────────────────┘
```

### The dependency rule

| Target | May depend on | May **never** depend on |
|---|---|---|
| `typeit::core` | the C++ standard library, nothing else | FTXUI, SQLite, TOML, the filesystem, the system clock, `app`, `infra`, `tui` |
| `typeit::app` | `core` | FTXUI, SQLite, TOML, `infra`, `tui` |
| `typeit::infra` | `core`, `app` (for the port declarations), SQLite, toml++ | FTXUI, `tui` |
| `typeit::tui` | `core`, `app`, FTXUI | SQLite, toml++, `infra` |
| `typeit::cli` | `core`, `app` | FTXUI, SQLite |
| `apps/typeit` | everything | — |

This is enforced mechanically, not by review: each library is a separate CMake target and
only its permitted dependencies are on its `target_link_libraries`. If you write
`#include <ftxui/...>` inside `core`, the build fails because FTXUI's include directories are
not on that target. See [BUILD.md §3](BUILD.md).

---

## 3. Architecture Decision Records

Each ADR states the decision, why, and what it costs.

### ADR-001 — `typeit::core` has zero third-party dependencies

**Decision.** The domain layer links only against the standard library. No FTXUI type ever
appears in a core header or source file.

**Why.** Today `Text` and `InputLineEngine` store `ftxui::Elements`
([review §3.3](CODEBASE_REVIEW.md)). That single fact makes the domain untestable headlessly,
unserialisable, and — because wrapping is computed once at construction — unable to survive a
terminal resize. Removing the dependency makes the model a plain data structure that can be
simulated, replayed, snapshotted, and re-wrapped freely.

**Cost.** The TUI layer must translate domain state into FTXUI elements every frame. That
translation is cheap and is a pure function, which makes it snapshot-testable.

---

### ADR-002 — The keystroke log is the single source of truth; all metrics are derived

**Decision.** A run records an append-only `KeystrokeLog` of timestamped events. Every metric
— WPM, accuracy, consistency, per-key latency, error pairs — is a **pure function over that
log plus the target text**. No metric is accumulated incrementally during play.

**Why.** The current accuracy bug ([review C4](CODEBASE_REVIEW.md)) exists precisely because
accuracy is accumulated: `push_character_accuracy()` appends on every keystroke and
`remove_element()` never removes, so a corrected mistake is permanently punished and holding
backspace inflates the denominator without bound. Deriving accuracy from the log makes that
class of bug *unrepresentable* — there is exactly one place where "what does accuracy mean"
is decided, and it can be tested against synthetic logs with no UI at all.

It also buys three features for free: per-second timeline charts, session replay, and offline
recomputation of a metric whose definition we later improve.

**Cost.** Memory proportional to keystrokes (a 60-second run at 100 WPM is roughly 500
events, ~16 bytes each — negligible). Metrics are computed on demand; the live HUD
recomputes over a rolling window rather than the whole log, which is O(window).

---

### ADR-003 — Rendering is a pure function of state

**Decision.** Model mutation happens **only** in event handlers. Render functions read state
and return elements. A render function that mutates anything is a bug.

**Why.** `Input::get_input_component()` currently advances the game inside its `Renderer`
lambda ([review §3.2](CODEBASE_REVIEW.md)), driven ten times a second by a background thread,
so the model's behaviour depends on the frame rate. That is unarguable — it must go.

**Cost.** A frame ticker is still needed for the countdown clock and the race pacer, but it
posts an event that a *handler* consumes, and the handler advances time explicitly. Time
becomes an input to the model, not an ambient property.

---

### ADR-004 — Text is a sequence of grapheme clusters, not bytes

**Decision.** `core::TextBuffer` stores a `std::vector<Grapheme>` where a `Grapheme` is a
short UTF-8 byte sequence forming one user-perceived character, tagged with its display
width (1 for normal, 2 for East Asian wide/emoji, 0 for combining marks). All indexing,
comparison, and cursor movement operates on grapheme indices.

**Why.** The current model compares `char` to `char`, so `č` (2 bytes) desynchronises the
accuracy vector and the line index. This is currently documented in the README as an FTXUI
limitation; it is substantially our own model's limitation. The user's headline requirement
is "type any text I want" — that includes accented Latin, Cyrillic, CJK, and emoji.

**Cost.** A segmentation pass at import time and a small grapheme-break table. See §6.2 for
the scope boundary.

---

### ADR-005 — No global mutable state; dependencies are injected through constructors

**Decision.** `GameState`, `GameOptions`, and `FocusPosition` are deleted. Run state lives in
a `Session` object. Configuration is a `Config` value passed where needed. Cursor/scroll
position is view state owned by the view. `static` is permitted only for `constexpr` data.

**Why.** See [review §3.1](CODEBASE_REVIEW.md). Globals make the test suite order-dependent
today and make replay, simulation, and concurrent sessions impossible.

**Cost.** More constructor parameters and explicit wiring in the composition root. That is the
point: the wiring becomes visible and reviewable instead of implicit.

---

### ADR-006 — Persistence: SQLite for history, TOML for configuration, at OS-standard paths

**Decision.** History and the text library live in a single SQLite database. User settings
live in a hand-editable TOML file. Both are located through a `PlatformPaths` adapter
following the XDG Base Directory spec on Linux and `%APPDATA%`/`%LOCALAPPDATA%` on Windows.
Bundled corpora are installed to the platform data directory and located at runtime via a
search path.

**Why.** History with trends, personal bests, per-key error heatmaps, and drill generation is
a *query* problem; SQLite is the right tool and adds no service to run. Configuration is
something users edit by hand, so it must be a readable text file, not a DB row.
`PlatformPaths` also kills the `__FILE__` asset lookup
([review C2](CODEBASE_REVIEW.md)) that currently makes the binary non-relocatable.

**Cost.** Two dependencies (`SQLite3`, `toml++`), both confined to `typeit::infra` behind
ports, both trivially replaceable.

---

### ADR-007 — Dependencies via `FetchContent` with `find_package` fallback; no vendored vcpkg

**Decision.** The vcpkg git submodule is removed. `cmake/Dependencies.cmake` uses
`FetchContent_Declare(... FIND_PACKAGE_ARGS ...)` so CMake prefers a system/distro package and
otherwise downloads and builds the dependency. `vcpkg.json` is retained so vcpkg users can opt
in with a toolchain file, but nothing requires it.

**Why.** A ~600 MB submodule for two dependencies is a poor trade, and it does not currently
work from a fresh clone ([review D7](CODEBASE_REVIEW.md)). `FIND_PACKAGE_ARGS` gives distro
packagers, vcpkg users, and plain `git clone && cmake --preset` users one code path.

**Cost.** First configure without system packages downloads and builds FTXUI/GTest/SQLite.
Mitigated by `FETCHCONTENT_BASE_DIR` caching and a CI dependency cache.

---

### ADR-008 — Game modes are strategies behind one interface

**Decision.** `core::IMode` defines the lifecycle hooks (`on_start`, `on_keystroke`,
`on_tick`, `is_finished`, `progress`). `TimedMode`, `WordCountMode`, `QuoteMode`,
`EndlessMode`, `RaceMode`, `DrillMode`, and `ZenMode` implement it. The session engine has no
`switch` on mode.

**Why.** Endless and Race are not variations of the current hardcoded countdown — they have
different termination conditions, different text supply, and (for Race) their own controller.
Special-casing them in the session loop would reproduce the coupling we are removing.

**Cost.** One indirection. Adding a mode becomes: implement the interface, register it, write
its tests.

---

### ADR-009 — `std::expected` for expected failures; exceptions for broken invariants

**Decision.** Operations that can fail for reasons the user can cause — missing file, malformed
config, corrupt database, invalid keybinding — return `std::expected<T, Error>` where `Error`
is a domain enum plus context. Exceptions signal programmer error and are not caught for
control flow. `catch (...) {}` is forbidden.

**Why.** `ComponentOptions` currently contains `catch (const std::invalid_argument& _) {}` — a
silently swallowed error inside a render transform. Making failure part of the return type
makes it impossible to ignore, since the result is `[[nodiscard]]`.

**Cost.** C++23 (`<expected>`); see §6.1 for the compiler floor.

---

### ADR-010 — Strong types for domain quantities

**Decision.** `Wpm`, `Accuracy`, `GraphemeIndex`, `Millis`, `SessionId` are distinct types,
not `int`/`float`/`size_t`. Mixed signed/unsigned arithmetic at container boundaries is
eliminated by construction.

**Why.** [Review C7](CODEBASE_REVIEW.md): `std::size_t` compared against `int`, where an empty
text turns `size() - 1` into `SIZE_MAX` and an out-of-bounds write is prevented only by an
unrelated early return. Strong types plus `-Wsign-conversion` make that unwritable.

**Cost.** Boilerplate, contained in one small header.

---

### ADR-011 — Do not use `ftxui::Input`; consume key events directly

**Decision.** The typing area is a custom `ftxui::ComponentBase` that handles
`Event::Character`, `Event::Backspace`, and friends itself, and forwards them to the domain
model. There is no `std::string` bound to an FTXUI input widget.

**Why.** The current design binds `ftxui::Input` to `input_text_` and then infers "the
character just typed" from `input_text_.back()`
([review T6](CODEBASE_REVIEW.md)). That is unsound for multi-byte input and paste, and it is
the actual source of the ćčšđž problem the README attributes to FTXUI. Since ADR-002 means we
already own the authoritative event log, the widget's own buffer is redundant state that can
only disagree with ours.

**Cost.** We handle key decoding ourselves. FTXUI already delivers whole UTF-8 sequences in
`Event::character()`, so this is less work than it sounds, and it gives us paste handling,
IME tolerance, and rebindable keys for free.

---

### ADR-012 — One frame ticker, adaptive rate, single-threaded model

**Decision.** Exactly one background thread posts `Event::Custom` at an adaptive rate:
~60 ms during an active run, ~500 ms when idle in a menu. The domain model is documented as
single-threaded and is only ever touched from the FTXUI event loop thread. Persistence writes
happen synchronously at session end inside one transaction.

**Why.** The current fixed 100 ms poll burns CPU in menus and couples model updates to frame
timing. Making the model single-threaded by contract removes a whole category of bug, and a
session write is a handful of inserts — measured in microseconds, not worth async complexity.

**Cost.** A very large history export could block a frame; export is therefore an explicit,
progress-reported operation rather than something that happens implicitly.

---

### ADR-013 — Ingestion is a three-stage pipeline, separate from text supply

**Decision.** Getting content *into* the library (acquire → extract → normalise) and serving
text *during a run* (`ITextProvider`) are different concerns with different interfaces, layers,
and lifetimes. See [TEXT_SOURCES.md](TEXT_SOURCES.md).

**Why.** The original `ITextSource` had one method, `get_text()`, which quietly conflated
"where do bytes come from" with "what does the typist see next". That works for one plain-text
file and for nothing else. Supporting web pages, EPUBs, subtitles, and source code requires
format-specific *extraction* that has nothing to do with how text is streamed during play — and
conversely, endless mode's word-pool generation should be identical whether the text came from
a file or a website.

Separating them means adding EPUB support is one new `ITextExtractor` and adding web support is
one new `IContentFetcher`, with neither touching the other and neither touching `core`.

**Cost.** Three interfaces where there was one, and a registry to resolve extractors by MIME
type. Justified the moment a second format exists.

---

### ADR-014 — Network access is optional at build time and opt-in at runtime

**Decision.** `TYPEIT_ENABLE_NETWORK` compiles the HTTP fetcher in or out; with it off, libcurl
is not a dependency at all. At runtime `[network] enabled` defaults to `false`. TypeIt makes no
request the user did not initiate, fetches exactly one URL per import, and **never follows
links**.

**Why.** Web import is genuinely useful and is also the only feature that gives this program a
network surface, a TLS dependency, and an interest in what is on the other end of a socket.
Making it removable keeps the default build strictly local, lets packagers ship a minimal
binary, and makes the promise "no telemetry, no update checks, nothing phones home" verifiable
by inspecting the link line rather than by trusting a sentence in a README.

The no-link-following rule is a hard boundary: a typing trainer that crawls is a different and
much worse program, and the distance between "fetch one page" and "fetch the pages it links to"
is one tempting commit.

**Cost.** A build option to test in both states, and a feature that is absent in some builds —
reported clearly by `--doctor` rather than failing mysteriously.

---

### ADR-015 — Heavy formats use an external converter hook, not a bundled parser

**Decision.** Built-in extractors cover the cheap formats (plain text, Markdown, HTML, EPUB,
subtitles, code, DOCX). Everything else routes through a user-configured external command —
`pdftotext`, `pandoc`, `ebook-convert` — declared in `[import.converters]`.

**Why.** A bundled PDF parser means poppler or pdfium: tens of megabytes, a large attack
surface, a real cross-platform build burden, and mediocre extraction anyway, because PDF
describes pages rather than paragraphs. Meanwhile `pandoc` already handles roughly forty
formats and is one line of configuration away. Inheriting all of them for nothing is a better
trade than writing any one parser.

**Cost.** The feature depends on tools TypeIt does not ship, so it degrades to "not available"
on a bare system — reported by `--doctor`. The command is executed as a subprocess, so it is
taken **only** from the user's own configuration, never from imported content, a filename, or a
URL, and it is passed as an argv array rather than through a shell.

---

## 4. Module map

### 4.1 `typeit::core` — domain

```
core/
├─ text/
│  ├─ Grapheme.h            Grapheme cluster + display width
│  ├─ TextBuffer.h/.cpp     UTF-8 → graphemes; indexing; slicing
│  ├─ Segmenter.h/.cpp      UTF-8 decode + grapheme break rules
│  └─ Wrapper.h/.cpp        pure: (graphemes, width) → line breaks
├─ session/
│  ├─ Keystroke.h           {timestamp, kind, grapheme, target_index}
│  ├─ KeystrokeLog.h/.cpp   append-only event log
│  ├─ TypingModel.h/.cpp    cursor + per-grapheme state machine
│  ├─ Session.h/.cpp        owns model + log + mode; the run
│  └─ SessionResult.h       immutable summary produced at the end
├─ metrics/
│  ├─ Metrics.h/.cpp        pure derivations over KeystrokeLog
│  ├─ Timeline.h/.cpp       per-second samples for charts
│  ├─ KeyStats.h/.cpp       per-grapheme / per-bigram latency + errors
│  └─ ErrorMap.h/.cpp       expected→typed substitution counts
├─ modes/
│  ├─ IMode.h               the strategy interface (ADR-008)
│  ├─ TimedMode, WordCountMode, QuoteMode, ZenMode
│  ├─ EndlessMode
│  ├─ RaceMode.h/.cpp       + Pacer, DifficultyController
│  └─ DrillMode.h/.cpp
├─ text_supply/
│  ├─ ITextProvider.h       successor to ITextSource
│  ├─ WholeTextProvider, ChunkedProvider, ShuffledSentenceProvider
│  └─ WordPoolProvider      infinite stream in the style of any text
├─ config/
│  ├─ Config.h              the full settings value type
│  └─ Validation.h/.cpp     pure validation → std::expected
└─ util/
   ├─ Result.h              std::expected aliases + Error
   ├─ Units.h               Wpm, Accuracy, Millis, GraphemeIndex
   └─ IClock.h              the time port
```

### 4.2 `typeit::app` — use cases

```
app/
├─ ports/
│  ├─ IHistoryRepository.h      sessions, samples, aggregates, PBs
│  ├─ ITextLibraryRepository.h  imported texts, tags, bookmarks, sections
│  ├─ IConfigStore.h            load/save Config
│  ├─ IAssetLocator.h           find bundled corpora
│  ├─ IContentFetcher.h         file / stdin / paste / URL      (ADR-013)
│  ├─ ITextExtractor.h          format → plain text + sections  (ADR-013)
│  └─ IFileSystem.h             read/exists/list, injectable for tests
├─ ExtractorRegistry.h/.cpp     MIME → extractor resolution
├─ SessionService.h/.cpp        build → run → summarise → persist
├─ HistoryService.h/.cpp        trends, PBs, streaks, filtering, export
├─ TextLibraryService.h/.cpp    import pipeline, score, tag, bookmark
├─ ProfileService.h/.cpp        adaptive baseline, level progression
├─ DrillService.h/.cpp          weak keys/bigrams → generated drill text
└─ ConfigService.h/.cpp         load, validate, migrate, save
```

### 4.3 `typeit::infra` — driven adapters

```
infra/
├─ db/  SqliteDatabase, Migrator, SqliteHistoryRepository,
│       SqliteTextLibraryRepository, schema/00N_*.sql
├─ config/  TomlConfigStore
├─ fs/  StdFileSystem, PlatformPaths, AssetLocator
├─ fetch/  FileFetcher, StdinFetcher, DirectoryFetcher,
│          HttpFetcher            (only if TYPEIT_ENABLE_NETWORK — ADR-014)
├─ extract/  PlainText, Markdown, Html, Epub, Subtitle, Code, Docx,
│            ExternalCommandExtractor                          (ADR-015)
└─ time/  SystemClock
```

The fetchers and extractors live in `infra` because they touch the filesystem, the network, and
third-party parsers. The *pipeline* that sequences them lives in `app`, and the normalisation
they feed is pure and lives in `core`.

### 4.4 `typeit::tui` — driving adapter

```
tui/
├─ TerminalApp.h/.cpp        owns ScreenInteractive, router, frame ticker
├─ ScreenStack.h/.cpp        push/pop navigation; replaces boolean flags
├─ screens/  MenuScreen, SessionScreen, ResultsScreen, HistoryScreen,
│            TextLibraryScreen, SettingsScreen, HelpScreen,
│            TerminalTooSmallScreen
├─ widgets/  TypingArea (ADR-011), StatsBar, PacerBar, Sparkline,
│            Histogram, Heatmap, TextList, KeyHintBar
├─ theme/    Theme, ThemeLoader, ColorQuantizer, GlyphSet, Capabilities
└─ input/    Keymap, KeyBindingParser
```

### 4.5 `apps/typeit` — composition root

`main.cpp` does exactly four things: parse arguments, construct every adapter, inject them,
and run. It is the only translation unit that names all four layers. Roughly 100 lines.

---

## 5. Runtime flow

### 5.1 Startup

```
main
 ├─ Cli::parse(argc, argv)                → CliOptions | help | version
 ├─ PlatformPaths::resolve()              → config/data/cache dirs
 ├─ TomlConfigStore::load()               → expected<Config, Error>
 │    └─ on error: report, continue with defaults, do not overwrite the file
 ├─ SqliteDatabase::open(data_dir/typeit.db)
 │    └─ Migrator::migrate_to_latest()    → expected<void, Error>
 ├─ construct repositories, clock, asset locator
 ├─ construct services (app layer)
 └─ TerminalApp(services, config).run()   or  Cli::run_headless(...)
```

### 5.2 A typing run

```
SessionScreen                 SessionService              core::Session
     │                              │                           │
     │ start(mode, text selection)  │                           │
     ├─────────────────────────────►│ resolve provider          │
     │                              ├──────────────────────────►│ construct
     │◄─────────── Session handle ──┤                           │
     │                              │                           │
 [key event]                        │                           │
     ├──────── on_key(Keystroke) ───────────────────────────────►│ append to log
     │                                                          │ advance model
     │◄──────────────────── ViewState (pure snapshot) ──────────┤
     │ render(ViewState)                                        │
     │                                                          │
 [tick event, ~60ms]                                            │
     ├──────── on_tick(now) ────────────────────────────────────►│ mode.on_tick
     │                                                          │ pacer advance
     │◄──────────────────── ViewState ──────────────────────────┤
     │                                                          │
 [mode reports finished]                                        │
     ├──────── finish() ───────────►│ Metrics::compute(log)     │
     │                              ├─► IHistoryRepository::    │
     │                              │   save_run(record, keys,  │
     │                              │            errors)        │
     │                              │   — one transaction: the  │
     │                              │   session, its samples,   │
     │                              │   its records and the     │
     │                              │   lifetime totals         │
     │◄────────── SessionResult ────┤                           │
     │ push ResultsScreen                                       │
```

The critical property: **`core::Session` never touches FTXUI, the clock, or the database.**
Time arrives as a parameter to `on_tick`. Persistence happens above it. That is what makes a
headless run — `typeit simulate --script keys.txt` — the same code path as a real one.

### 5.3 Navigation

`ScreenStack` replaces the current five booleans in `GameState`
(`game_session_in_progress`, `start_session`, `refresh_session`, `game_finished`,
`show_info`). Screens are pushed and popped; the top of the stack renders and receives
events. "Restart" pops and pushes a fresh `SessionScreen`; "back to menu" pops. There is no
global flag anyone can flip from three layers away.

---

## 6. Cross-cutting decisions

### 6.1 Language level and compiler floor

**C++23**, for `std::expected` (ADR-009), `std::print`/`std::format` improvements, and
`std::ranges` conveniences.

| Toolchain | Minimum |
|---|---|
| GCC | 13 |
| Clang | 17 (with libc++ 17 or libstdc++ 13) |
| MSVC | 19.38 (Visual Studio 2022 17.8) |
| Apple Clang | not a supported target |

The local toolchain here is GCC 16.1.1, comfortably above the floor. If a supported platform
turns out to lack `<expected>`, the fallback is a ~120-line `core::Expected` shim behind the
same alias in `util/Result.h` — a contained change, which is why the alias exists.

### 6.2 Unicode scope

**In scope:** UTF-8 decoding; combining marks (`e` + U+0301); East Asian wide characters at
width 2; emoji including ZWJ sequences and regional-indicator pairs as single graphemes;
CRLF as one break.

**Out of scope:** full UAX #29 with the complete break-property tables, bidirectional text,
and complex-script shaping (Devanagari, Arabic joining). Attempting those in a terminal typing
test is not worthwhile; the boundary is documented so that a bug report about Arabic gets a
clear, honest answer instead of a silent failure.

### 6.3 Error handling

- `std::expected<T, Error>` for anything the user can cause.
- `Error` carries a code enum, a human message, and optional context (path, line, SQL).
- Exceptions cross no layer boundary; `main` has one top-level handler that prints a
  diagnostic and exits non-zero.
- Assertions guard internal invariants and are enabled in debug builds.
- **The application never silently swallows an error.** Failures that cannot be surfaced in
  the UI go to a log file under the platform cache directory.

### 6.4 Threading contract

| Thread | Owns |
|---|---|
| Main | FTXUI event loop, all screens, `core::Session`, all services, all repositories |
| Frame ticker | Nothing. Posts `Event::Custom`. Reads one `std::atomic<bool>` and one `std::atomic<int>` (interval). |

Every class documents its thread affinity in its header. There is no shared mutable state
beyond those two atomics.

### 6.5 Performance budget

| Operation | Budget |
|---|---|
| Keystroke → updated frame | < 5 ms |
| Frame render (whole screen) | < 3 ms |
| Metrics over a rolling window | O(window), not O(log) |
| Session save | < 20 ms, one transaction |
| History screen, 10k sessions | < 200 ms, aggregation in SQL |
| Idle CPU in a menu | < 0.5% |

### 6.6 Extension points

Adding these should require no change to existing code:

| Extension | How |
|---|---|
| A new game mode | Implement `IMode`, register in the mode factory |
| A new text source | Implement `ITextProvider` |
| A new metric | Add a pure function in `metrics/`, add a column, add a migration |
| A new theme | Drop a `.toml` in the themes directory |
| A different storage backend | Implement `IHistoryRepository` |
| A different frontend | Implement against `typeit::app` — nothing in `core`/`app` prevents it, though no such frontend is planned |

---

## 7. Directory layout

```
TypeIt/
├─ CMakeLists.txt
├─ CMakePresets.json
├─ cmake/                    Dependencies, CompilerWarnings, Sanitizers, StaticAnalysis
├─ apps/typeit/              main.cpp — composition root
├─ libs/
│  ├─ core/    include/typeit/core/…    src/
│  ├─ app/     include/typeit/app/…     src/
│  ├─ infra/   include/typeit/infra/…   src/
│  ├─ tui/     include/typeit/tui/…     src/
│  └─ cli/     include/typeit/cli/…     src/
├─ tests/      core/  app/  infra/  tui/  e2e/
├─ assets/
│  ├─ texts/       bundled corpora, installed to the data dir
│  └─ themes/      builtin theme .toml files
├─ docs/           this directory
└─ .github/workflows/
```

Includes are always target-qualified — `#include "typeit/core/session/Session.h"` — never
relative. This is what makes an illegal dependency a compile error rather than a review
comment.
