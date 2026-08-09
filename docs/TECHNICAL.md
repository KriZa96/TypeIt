# TypeIt — Technical Specification

*The concrete shape of the code: key types, algorithms, the database schema, the config
format, and the CLI. Companion to [ARCHITECTURE.md](ARCHITECTURE.md) (which explains why) and
[GAMEPLAY.md](GAMEPLAY.md) (which defines the behaviour).*

---

## 1. Core domain types

### 1.1 Units (`core/util/Units.h`)

Strong types, per ADR-010. Each is a single-member struct with explicit construction and only
the arithmetic that makes sense for it.

```cpp
namespace typeit::core {

struct Wpm            { double value; };          // words per minute
struct Accuracy       { double value; };          // 0.0 – 1.0
struct Millis         { std::int64_t value; };    // duration or timestamp, ms
struct GraphemeIndex  { std::size_t value; };     // position in a TextBuffer
struct SessionId      { std::int64_t value; };
struct TextId         { std::int64_t value; };

}  // namespace typeit::core
```

`GraphemeIndex` replaces every place the current code mixes `int` and `std::size_t`, which is
the root of [defect C7](CODEBASE_REVIEW.md#4-correctness-defects).

### 1.2 `Result` (`core/util/Result.h`)

```cpp
enum class ErrorCode {
    FileNotFound, FileUnreadable, InvalidUtf8, EmptyText, TextTooLarge,
    ConfigParse, ConfigInvalid, DbOpen, DbMigrate, DbQuery,
    UnknownTheme, InvalidKeyBinding, UnsupportedTerminal,
};

struct Error {
    ErrorCode   code;
    std::string message;      // human-readable, already formatted
    std::string context;      // path, SQL, line number — optional
};

template <typename T> using Result = std::expected<T, Error>;
using Status = std::expected<void, Error>;
```

Every `Result` is `[[nodiscard]]` by virtue of `std::expected`. This is the only error
mechanism crossing a layer boundary (ADR-009).

### 1.3 `IClock` (`core/util/IClock.h`)

```cpp
class IClock {
public:
    virtual ~IClock() = default;
    [[nodiscard]] virtual Millis now() const = 0;
};
```

`SystemClock` in `infra`; `FakeClock` in tests. This is what removes every `sleep_for` from the
test suite ([review §6](CODEBASE_REVIEW.md#6-test-suite-problems)) — the timer tests become
instant and exact instead of sleeping nine seconds and asserting on a race.

### 1.4 Text (`core/text/`)

```cpp
struct Grapheme {
    std::array<char, 12> bytes;   // UTF-8, inline — no allocation
    std::uint8_t         length;  // bytes in use
    std::uint8_t         width;   // display columns: 0, 1, or 2

    [[nodiscard]] std::string_view view() const noexcept;
    [[nodiscard]] bool operator==(const Grapheme&) const noexcept;
};

class TextBuffer {
public:
    static Result<TextBuffer> from_utf8(std::string_view text);

    [[nodiscard]] std::size_t         size() const noexcept;
    [[nodiscard]] const Grapheme&     at(GraphemeIndex) const;
    [[nodiscard]] std::span<const Grapheme> graphemes() const noexcept;
    [[nodiscard]] std::string         to_string(GraphemeIndex from, GraphemeIndex to) const;
    [[nodiscard]] std::size_t         word_count() const noexcept;
};
```

`Grapheme` is 14 bytes and trivially copyable, so a `TextBuffer` is one contiguous allocation.
Twelve bytes covers every sequence in scope per
[ARCHITECTURE §6.2](ARCHITECTURE.md#62-unicode-scope); longer clusters are truncated at the
segmentation boundary and reported.

**Segmentation algorithm** (`Segmenter`): decode UTF-8 to code points, then join adjacent code
points into a cluster when any of the following hold — the following code point is a combining
mark (`Mn`/`Mc`/`Me`), the pair is joined by U+200D ZWJ, the pair forms a regional-indicator
sequence, the following code point is a variation selector, or the pair is CR followed by LF.
Width is 2 for East Asian Wide/Fullwidth ranges and for emoji presentation, 0 for combining
marks, 1 otherwise. The property tables live in a generated header so they can be regenerated
from Unicode data files rather than hand-maintained.

**Wrapping** (`Wrapper`) is a *pure function*, not baked into the buffer:

```cpp
struct LineBreaks { std::vector<GraphemeIndex> starts; };

[[nodiscard]] LineBreaks wrap(std::span<const Grapheme> text, std::size_t columns);
```

Greedy wrapping at the last space that fits, accounting for display width; a word longer than
the line is hard-broken. Because this is a pure function of (text, columns) rather than state
computed once at construction, **terminal resize is handled by calling it again** — the defect
described in [review §3.3](CODEBASE_REVIEW.md#33-ftxui-types-live-in-the-domain).

### 1.5 The keystroke log (`core/session/`)

```cpp
enum class KeystrokeKind : std::uint8_t { Type, Backspace, Skip };

struct Keystroke {
    Millis        at;             // ms since run start
    KeystrokeKind kind;
    Grapheme      typed;          // meaningless for Backspace
    GraphemeIndex target;         // position it applied to
};

class KeystrokeLog {
public:
    void append(const Keystroke&);
    [[nodiscard]] std::span<const Keystroke> events() const noexcept;
    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] Millis duration() const noexcept;
};
```

Append-only. Never mutated, never rewound. This is the source of truth (ADR-002) and the
reason the accuracy defect cannot recur.

### 1.6 The typing model

```cpp
enum class GraphemeState : std::uint8_t { Pending, Correct, Incorrect, Corrected, Missed };

class TypingModel {
public:
    TypingModel(const TextBuffer& target, TypingRules rules);

    // The only mutators. Called from event handlers only (ADR-003).
    void type(const Grapheme&, Millis at);
    void backspace(Millis at);

    [[nodiscard]] GraphemeIndex cursor() const noexcept;
    [[nodiscard]] std::span<const GraphemeState> states() const noexcept;
    [[nodiscard]] const KeystrokeLog& log() const noexcept;
    [[nodiscard]] bool at_end() const noexcept;
};
```

Transitions follow the state machine in [GAMEPLAY §6](GAMEPLAY.md#6-typing-rules). The model
holds no FTXUI type, no clock, no I/O — time arrives as a parameter.

### 1.7 Metrics

```cpp
struct SessionMetrics {
    Wpm raw_wpm, gross_wpm, net_wpm, peak_sustained_wpm;
    Accuracy accuracy, final_correctness;
    double consistency;
    std::size_t graphemes_typed, graphemes_correct;
    std::size_t errors_total, errors_uncorrected, backspaces;
    Millis duration, time_to_first_keystroke;
};

[[nodiscard]] SessionMetrics compute(const KeystrokeLog&, const TextBuffer&, Millis end);
[[nodiscard]] Timeline      timeline(const KeystrokeLog&, const TextBuffer&, Millis bucket);
[[nodiscard]] KeyStats      key_stats(const KeystrokeLog&, const TextBuffer&);
[[nodiscard]] ErrorMap      error_map(const KeystrokeLog&, const TextBuffer&);
[[nodiscard]] Wpm rolling_wpm(const KeystrokeLog&, const TextBuffer&, Millis now, Millis window);
```

Free functions over immutable inputs. Every formula is specified in
[GAMEPLAY §4](GAMEPLAY.md#4-metrics--exact-definitions) and each has a unit test built from a
synthetic log — no UI, no clock, no database.

`rolling_wpm` maintains an index into the log so the live HUD is O(window) rather than
O(log), meeting the budget in
[ARCHITECTURE §6.5](ARCHITECTURE.md#65-performance-budget).

### 1.8 Modes

```cpp
class IMode {
public:
    virtual ~IMode() = default;

    virtual void on_start(Millis at, const TypingModel&) = 0;
    virtual void on_keystroke(const Keystroke&, const TypingModel&) = 0;
    virtual void on_tick(Millis now, const TypingModel&) = 0;

    [[nodiscard]] virtual bool          is_finished() const = 0;
    [[nodiscard]] virtual ModeProgress  progress() const = 0;   // for the HUD
    [[nodiscard]] virtual std::string_view id() const = 0;
};
```

`ModeProgress` is a small variant carrying whatever the HUD must draw: remaining seconds,
words done/total, or race state (pacer position, target WPM, lead, lives).

### 1.9 Race internals

```cpp
class Pacer {
public:
    void  reset(Wpm start, Millis grace);
    void  advance(Millis now);
    void  push_back(GraphemeIndex to);          // on losing a life
    [[nodiscard]] double position() const;      // fractional grapheme index
    [[nodiscard]] Wpm    speed() const;
    void  set_speed(Wpm);
};

class DifficultyController {
public:
    explicit DifficultyController(RaceParams);
    // Implements the ramp law in GAMEPLAY §3.2. Pure arithmetic, no I/O.
    [[nodiscard]] Wpm next_speed(Wpm current, double lead, Accuracy rolling, Millis delta) const;
};
```

`DifficultyController` is pure arithmetic, which makes the whole ramp testable by feeding it
scripted (lead, accuracy) sequences and asserting on the resulting speed curve — including the
properties that matter: monotone response to lead, no gain below `A_min`, hysteresis inside
the dead band, and recovery after a stumble.

---

## 2. Application layer

### 2.1 Ports

```cpp
class IHistoryRepository {
public:
    virtual ~IHistoryRepository() = default;
    virtual Result<SessionId>              save(const SessionRecord&)                     = 0;
    // A finished run, whole, in one transaction — TI-068. Three separate calls
    // cannot be made atomic from above, and a partial write is unrecoverable:
    // merging the stats again would double-count the run that did land.
    virtual Result<SessionId>              save_run(const SessionRecord&, const KeyStats&,
                                                    const ErrorMap&)                      = 0;
    virtual Result<std::vector<SessionRow>> query(const HistoryFilter&)  const            = 0;
    virtual Result<Aggregates>             aggregates(const HistoryFilter&) const         = 0;
    virtual Result<std::vector<PersonalBest>> personal_bests()            const           = 0;
    virtual Status                         merge_key_stats(const KeyStats&)               = 0;
    virtual Result<KeyStats>               key_stats(const HistoryFilter&) const          = 0;
    virtual Result<Wpm>                    best_sustained_wpm(Days window) const          = 0;
};

class ITextLibraryRepository { /* add, list, get, remove, tag, bookmark, sections, find_by_hash */ };
class IConfigStore           { /* load, save */ };
class IAssetLocator          { /* locate("texts"), locate("themes") */ };
class IFileSystem            { /* read_text, exists, is_directory, list */ };

// Ingestion ports — ADR-013. See TEXT_SOURCES.md.
class IContentFetcher {
public:
    virtual ~IContentFetcher() = default;
    [[nodiscard]] virtual bool can_handle(std::string_view locator) const = 0;
    [[nodiscard]] virtual Result<FetchedContent> fetch(std::string_view locator) const = 0;
};

class ITextExtractor {
public:
    virtual ~ITextExtractor() = default;
    [[nodiscard]] virtual std::span<const std::string_view> mime_types() const = 0;
    [[nodiscard]] virtual Result<ExtractedText> extract(const FetchedContent&) const = 0;
};
```

`best_sustained_wpm` exists as a port method rather than as client-side filtering because it
is the input to race mode's starting speed ([GAMEPLAY §3.4](GAMEPLAY.md#34-starting-speed--where-progression-lives))
and belongs in SQL over a large history.

### 2.2 Services

| Service | Responsibility |
|---|---|
| `SessionService` | Resolve mode + text + provider, construct a `core::Session`, and on finish compute metrics, persist the record, merge key/bigram stats, update personal bests — in one transaction |
| `HistoryService` | Trends, aggregates, PBs, streaks, filtering, CSV/JSON export |
| `TextLibraryService` | Runs the ingestion pipeline — fetch → extract → normalise → typing-readiness — then hashes, deduplicates, scores difficulty, tags, and bookmarks. See [TEXT_SOURCES.md](TEXT_SOURCES.md) |
| `ProfileService` | Adaptive baseline for race start speed, daily goal, streak accounting |
| `DrillService` | Select weak targets from stats and synthesise drill text |
| `ConfigService` | Load, validate, apply defaults, migrate old keys, save |

Services take their ports by `std::shared_ptr` or reference in the constructor. No service
reaches for a global (ADR-005).

---

## 3. TUI layer

### 3.1 `TypingArea` (ADR-011)

A custom `ftxui::ComponentBase`. It owns no text buffer of its own; it renders from the model
and forwards events.

```cpp
class TypingArea : public ftxui::ComponentBase {
public:
    TypingArea(const TypingModel& model, const Theme& theme, TypingAreaOptions);

    ftxui::Element Render() override;         // pure: state → elements
    bool OnEvent(ftxui::Event) override;      // the ONLY place the model mutates
    bool Focusable() const override { return true; }

private:
    const TypingModel& model_;
    const Theme&       theme_;
    LineBreaks         breaks_;               // recomputed on resize
    std::size_t        last_columns_ = 0;
};
```

`OnEvent` maps `Event::Character` (already a complete UTF-8 sequence from FTXUI) to
`model_.type(...)`, `Event::Backspace` to `model_.backspace(...)`, and returns `false` for
everything else so the parent can handle navigation. **`Render()` mutates nothing** — the one
rule the current implementation breaks.

Rendering draws the target text with per-grapheme state colouring, the caret in the configured
style, and — in race mode — the pacer marker. It re-runs `wrap()` when the column count
changes, which is the whole resize story.

### 3.2 Screen stack

```cpp
class ScreenStack {
public:
    void push(std::shared_ptr<IScreen>);
    void pop();                             // refused when it would empty the stack
    void replace(std::shared_ptr<IScreen>);
    [[nodiscard]] IScreen& top();

    [[nodiscard]] ftxui::Element render();  // the top screen, and only it
    [[nodiscard]] bool on_event(ftxui::Event);
};
```

Replaces the five booleans in the current `GameState`. Only the top screen renders and
receives events. `IScreen` exposes `render()`, `on_event()`, and `title()` — snake_case per
[STYLE §2](STYLE.md), unlike `TypingArea` above, whose `Render()` and `OnEvent()` are FTXUI's
names because it genuinely overrides `ftxui::ComponentBase`.

Two decisions the sketch does not show. **Popping the last screen is refused**: an empty stack
renders nothing and answers no key, so a program that popped its last screen would be a black
terminal that ignores the keyboard — leaving is `TerminalApp::quit()`, which says so.
**Mutation during dispatch is deferred** to the end of the frame: a screen that pushes another
from inside `on_event` is the ordinary case, and applying it immediately would destroy the
object whose method is still on the call stack.

`ScreenStack` and `IScreen` are internal to `typeit::tui` rather than part of its public
headers, because they name `ftxui::Element` and `ftxui::Event` and FTXUI is linked `PRIVATE`.
The library's public surface is `TerminalApp`: the composition root asks for an application,
not for a rendering vocabulary.

### 3.3 Theme and capability detection

```cpp
enum class ColorDepth { Mono, Ansi16, Ansi256, TrueColor };
enum class GlyphSet   { Ascii, Unicode };

struct Capabilities {
    ColorDepth color;
    GlyphSet   glyphs;
    bool       mouse;
    static Capabilities detect();      // env-based, overridable by config
};
```

Detection order: `NO_COLOR` → `Mono`; `COLORTERM` in {`truecolor`,`24bit`} → `TrueColor`;
`TERM` containing `256color` → `Ansi256`; `WT_SESSION` present → `TrueColor` + `Unicode`;
`TERM=linux` or `dumb` → `Ansi16` + `Ascii`; otherwise `Ansi16` + `Ascii` as the safe floor.
Config always wins over detection.

Themes are declared in truecolor and quantised down by `ColorQuantizer` (nearest colour in the
xterm-256 cube, then nearest of the 16 ANSI colours). This replaces the
`FTXUI_MICROSOFT_TERMINAL_FALLBACK` macro that
[is branched on but never defined](CODEBASE_REVIEW.md#7-terminal-portability-gaps) with runtime
detection that actually runs.

### 3.4 Frame ticker

```cpp
class FrameTicker {
public:
    FrameTicker(ftxui::ScreenInteractive&, std::chrono::milliseconds idle,
                                            std::chrono::milliseconds active);
    ~FrameTicker();                     // stops and joins; RAII, no manual stop needed
    void set_active(bool);
};
```

One thread, adaptive interval (ADR-012). Unlike the current `Screen`, the ticker's only job is
posting an event — it neither owns nor advances any model state.

---

## 4. Infrastructure

### 4.1 Platform paths

| | Linux | Windows |
|---|---|---|
| Config | `$XDG_CONFIG_HOME/typeit` (default `~/.config/typeit`) | `%APPDATA%\TypeIt` |
| Data | `$XDG_DATA_HOME/typeit` (default `~/.local/share/typeit`) | `%LOCALAPPDATA%\TypeIt` |
| Cache/logs | `$XDG_CACHE_HOME/typeit` | `%LOCALAPPDATA%\TypeIt\cache` |

`TYPEIT_CONFIG_DIR` and `TYPEIT_DATA_DIR` override both, which is what makes integration tests
hermetic.

**Asset lookup** replaces `__FILE__` ([defect C2](CODEBASE_REVIEW.md#4-correctness-defects)),
searched in order:

1. `$TYPEIT_ASSETS_DIR`
2. executable directory `/../share/typeit`
3. `${CMAKE_INSTALL_PREFIX}/share/typeit` (baked in at configure time)
4. `$XDG_DATA_DIRS` entries
5. `./assets` (development convenience only)

The first hit wins; if nothing is found the app reports it clearly and still runs with
user-imported texts.

What lives under the directory that wins:

| | |
|---|---|
| `themes/` | the shipped `.toml` themes; a user's own go in `$XDG_CONFIG_HOME/typeit/themes` and are searched first |
| `texts/` | the bundled corpora — `simple.txt`, `medium.txt`, `hard.txt`, which 1.0 kept in `files/` and found with `__FILE__` |

### 4.2 SQLite

- One connection, opened with `SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE`.
- `PRAGMA journal_mode = WAL; foreign_keys = ON; synchronous = NORMAL; busy_timeout = 3000;`
- All statements prepared once and cached; **no string-concatenated SQL anywhere** — every
  variable is a bound parameter.
- Every write path wrapped in a transaction via an RAII `Transaction` guard.
- Migrations keyed on `PRAGMA user_version`, applied in order, each in its own transaction.
  Schema SQL lives in versioned files embedded into the binary at build time.
- A corrupt or future-versioned database is reported, not silently recreated; the app offers to
  move it aside.

---

## 5. Database schema (v1)

```sql
PRAGMA user_version = 1;

CREATE TABLE profile (
    id            INTEGER PRIMARY KEY CHECK (id = 1),
    created_at    INTEGER NOT NULL,
    display_name  TEXT,
    daily_goal_ms INTEGER NOT NULL DEFAULT 600000
);

CREATE TABLE text_item (
    id             INTEGER PRIMARY KEY,
    title          TEXT    NOT NULL,
    source         TEXT    NOT NULL CHECK (source IN ('builtin','file','paste','stdin')),
    origin         TEXT,
    content        TEXT    NOT NULL,    -- normalised
    content_raw    TEXT,                -- as imported
    content_sha256 TEXT    NOT NULL UNIQUE,
    language       TEXT,
    grapheme_count INTEGER NOT NULL,
    word_count     INTEGER NOT NULL,
    difficulty     REAL,
    created_at     INTEGER NOT NULL
);

CREATE TABLE text_tag (
    text_id INTEGER NOT NULL REFERENCES text_item(id) ON DELETE CASCADE,
    tag     TEXT    NOT NULL,
    PRIMARY KEY (text_id, tag)
);

CREATE TABLE text_bookmark (
    text_id    INTEGER PRIMARY KEY REFERENCES text_item(id) ON DELETE CASCADE,
    offset     INTEGER NOT NULL,
    updated_at INTEGER NOT NULL
);

CREATE TABLE session (
    id                 INTEGER PRIMARY KEY,
    started_at         INTEGER NOT NULL,       -- unix ms
    ended_at           INTEGER NOT NULL,
    mode               TEXT    NOT NULL,
    mode_param         TEXT,                   -- JSON: {"seconds":30} / {"words":50} / race params
    text_id            INTEGER REFERENCES text_item(id) ON DELETE SET NULL,
    provider           TEXT    NOT NULL,
    provider_seed      INTEGER NOT NULL,       -- reproduces the exact text stream
    duration_ms        INTEGER NOT NULL,
    graphemes_typed    INTEGER NOT NULL,
    graphemes_correct  INTEGER NOT NULL,
    errors_total       INTEGER NOT NULL,
    errors_uncorrected INTEGER NOT NULL,
    backspaces         INTEGER NOT NULL,
    raw_wpm            REAL    NOT NULL,
    gross_wpm          REAL    NOT NULL,
    net_wpm            REAL    NOT NULL,
    accuracy           REAL    NOT NULL,
    final_correctness  REAL    NOT NULL,
    consistency        REAL    NOT NULL,
    peak_wpm           REAL,                   -- race: highest sustained pacer speed
    wall_wpm           REAL,                   -- race: speed at which accuracy collapsed
    completed          INTEGER NOT NULL CHECK (completed IN (0,1)),
    app_version        TEXT    NOT NULL
);
CREATE INDEX idx_session_started ON session(started_at DESC);
CREATE INDEX idx_session_mode    ON session(mode, started_at DESC);

CREATE TABLE session_sample (          -- per-second timeline for charts
    session_id INTEGER NOT NULL REFERENCES session(id) ON DELETE CASCADE,
    t_ms       INTEGER NOT NULL,
    wpm        REAL    NOT NULL,
    errors     INTEGER NOT NULL,
    pacer_wpm  REAL,
    PRIMARY KEY (session_id, t_ms)
) WITHOUT ROWID;

CREATE TABLE key_stat (                -- lifetime aggregate, merged per session
    grapheme         TEXT    PRIMARY KEY,
    attempts         INTEGER NOT NULL,
    errors           INTEGER NOT NULL,
    total_latency_ms INTEGER NOT NULL
);

CREATE TABLE bigram_stat (
    bigram           TEXT    PRIMARY KEY,
    attempts         INTEGER NOT NULL,
    errors           INTEGER NOT NULL,
    total_latency_ms INTEGER NOT NULL
);

CREATE TABLE error_pair (
    expected TEXT    NOT NULL,
    typed    TEXT    NOT NULL,
    count    INTEGER NOT NULL,
    PRIMARY KEY (expected, typed)
);

CREATE TABLE personal_best (
    mode        TEXT    NOT NULL,
    param       TEXT    NOT NULL,
    metric      TEXT    NOT NULL,       -- 'net_wpm' | 'peak_wpm' | 'accuracy'
    session_id  INTEGER NOT NULL REFERENCES session(id) ON DELETE CASCADE,
    value       REAL    NOT NULL,
    achieved_at INTEGER NOT NULL,
    PRIMARY KEY (mode, param, metric)
);

CREATE TABLE keystroke_blob (          -- optional full log for replay; off by default
    session_id INTEGER PRIMARY KEY REFERENCES session(id) ON DELETE CASCADE,
    encoding   TEXT NOT NULL,
    data       BLOB NOT NULL
);
```

Design notes:

- `provider_seed` makes any session's exact text stream reproducible, which turns a bug report
  into a deterministic repro.
- `session_sample` is `WITHOUT ROWID` because the composite primary key is the natural access
  path and there is no other index.
- `key_stat` / `bigram_stat` / `error_pair` are lifetime aggregates merged with
  `INSERT … ON CONFLICT DO UPDATE`. Per-session breakdowns are recoverable from
  `keystroke_blob` when enabled; storing per-session key stats for every run is not worth the
  space.
- `mode_param` is JSON rather than a column per mode, deliberately: adding a mode must not
  require a migration (ADR-008).

**user_version 3** (`003_sections.sql`) adds `text_section`, plus `section_idx` on bookmarks and
`author` / `mime` / `extractor` on `text_item`, to support chapters in imported ebooks and web
articles. It is specified in [TEXT_SOURCES §9](TEXT_SOURCES.md#9-sections-and-schema) and
delivered by
[TX-006](issues/PHASE-6A-text-sources.md#tx-006--schema-v2-and-section-persistence) — which
TEXT_SOURCES calls "schema v2", from before TI-109's daily-totals index took 2. An entity
diagram of both versions is in [DIAGRAMS §7](DIAGRAMS.md#7-database-schema).

---

## 6. Configuration file

`$XDG_CONFIG_HOME/typeit/config.toml`, written with commented defaults on first run.

```toml
# TypeIt configuration. Delete any key to fall back to its default.
config_version = 1

[general]
default_mode       = "timed"       # timed | words | quote | zen | endless | race
default_duration_s = 30
default_word_count = 50
countdown_s        = 3             # 0 disables
confirm_quit       = true
log_level          = "warn"        # off | error | warn | info | debug

[appearance]
theme          = "typeit-dark"     # builtin name, or a path to a .toml theme
color_depth    = "auto"            # auto | truecolor | 256 | 16 | mono
glyphs         = "auto"            # auto | unicode | ascii
caret          = "block"           # block | underline | outline | none
caret_blink    = false
layout         = "comfortable"     # compact | comfortable
line_width     = 0                 # graphemes per line; 0 = fit the terminal
lines_visible  = 3
show_live_wpm  = true
show_live_acc  = true
show_progress  = true
smooth_scroll  = true

[typing]
stop_on_error      = "off"         # off | letter | word
allow_backspace    = true
strict_spaces      = true
space_advances_word = true
blind_mode         = false
confidence_mode    = "off"         # off | on | max

[text]
flatten_typography = true          # smart quotes/dashes → ASCII
collapse_whitespace = true
strip_punctuation  = false
lowercase          = false
tab_width          = 4
chunk_graphemes    = 1200          # for long documents

[race]
preset             = "standard"    # gentle | standard | brutal | custom
start_policy       = "from_history"  # from_history | fixed
start_wpm          = 20
ramp_up            = 0.60          # WPM gained per second at full lead
ramp_down          = 1.50          # WPM lost per second when struggling
min_accuracy       = 0.92
lead_comfort       = 25            # graphemes
lead_danger        = 8
lead_scale         = 30
grace_ms           = 300
lives              = 1
sustain_window_s   = 10

[history]
keep_keystroke_logs = false        # enables exact replay; grows the database
retention_days      = 0            # 0 = keep everything

[network]                          # ADR-014; absent entirely if built with
enabled      = false               #   TYPEIT_ENABLE_NETWORK=OFF
allow_http   = false               # HTTPS only by default
allow_private_addresses = false    # localhost / RFC 1918 refused
timeout_s    = 30
max_size_mb  = 10
respect_robots_txt = true

[import]
dehyphenate        = true          # typing-readiness pass — TEXT_SOURCES §8
drop_running_heads = true
strip_footnotes    = true
rejoin_paragraphs  = true
report_unreachable_characters = true

[import.converters]                # ADR-015; argv execution, never a shell
# "application/pdf" = "pdftotext -layout -nopgbrk {input} -"
# "*"               = "pandoc --to=plain --wrap=none {input}"

[keys]
quit          = "ctrl-q"
back          = "escape"
restart       = "ctrl-r"
next_text     = "ctrl-n"
history       = "ctrl-h"
library       = "ctrl-l"
settings      = "f2"
help          = "f1"
```

Loading rules: unknown keys are reported as warnings and ignored (forward compatibility);
invalid values fall back to the default with a warning; a malformed file is reported and the
file is **never** silently overwritten. `config_version` drives migration of renamed keys.

Themes use the same format:

```toml
name = "typeit-dark"
[colors]
background = "#1e1e2e"
text_pending = "#6c7086"
text_correct = "#cdd6f4"
text_incorrect = "#f38ba8"
text_corrected = "#f9e2af"
caret = "#89b4fa"
pacer = "#a6e3a1"
accent = "#89b4fa"
border = "#45475a"
```

---

## 7. Command-line interface

```
typeit [OPTIONS] [FILE|-]

Modes
  -m, --mode <MODE>          timed | words | quote | zen | endless | race
  -t, --time <SECONDS>       duration for timed mode
  -w, --words <N>            word count for words mode
      --race-preset <P>      gentle | standard | brutal

Text
      --text <PATH>          type this file (one-shot; does not import)
      --text-id <ID>         type a text from the library
      --section <N>          start at section N of the selected text
  -                          read the text from stdin

Library
      --import <PATH>        import into the library and exit
      --import-dir <DIR>     import every supported file in a directory
      --url <URL>            fetch and import a web page (requires [network].enabled)
      --list-texts           list library texts and exit
      --remove-text <ID>

History
      --stats                print a summary and exit
      --export <FORMAT>      csv | json — write history to stdout
      --last <N>             limit history output

Diagnostics
      --simulate <SCRIPT>    run headless from a keystroke script, print metrics
      --doctor               report terminal capabilities, paths, database health
      --config <PATH>        use an alternative config file
      --data-dir <PATH>      use an alternative data directory
  -h, --help
  -V, --version
```

`--simulate` is the end-to-end test harness described in [TESTING.md](TESTING.md): it drives
the real `core::Session` from a scripted log with a `FakeClock` and prints the resulting
metrics, with no terminal involved. `--doctor` is the first thing to ask a user to run when
they report a rendering problem in an unfamiliar terminal.

---

## 8. Key algorithms

### 8.1 Rolling WPM (live HUD)

Maintain a left index into the log. On each tick, advance it past events older than
`now − window`, then compute gross WPM over the remaining slice. Amortised O(1) per tick, O(window)
memory. Never rescans the whole log — the property that keeps the HUD inside the 5 ms budget
in a long endless run.

### 8.2 Consistency

Bucket the log into one-second windows, compute gross WPM per bucket, then
`100 × (1 − σ/μ)` floored at 0. Buckets with no keystrokes count as 0 WPM, which is what makes
the metric detect pauses rather than ignore them.

### 8.3 Difficulty scoring at import

```
score = w1·norm(mean_word_length)
      + w2·norm(rare_word_ratio)
      + w3·norm(punctuation_density)
      + w4·norm(capital_density)
      + w5·norm(digit_density)
      + w6·norm(non_ascii_density)
```

Each term normalised to 0–1 against empirical bounds, weights summing to 1, result scaled to
1–10. Advisory only.

### 8.4 Drill target selection

```
priority(target) = error_rate(target)^1.5 × log(1 + attempts) × recency_weight
```

The `^1.5` exponent favours genuinely bad keys over marginally bad ones;
`log(1 + attempts)` suppresses targets with too little data to trust; `recency_weight` decays
over 30 days so an old weakness that has been fixed stops being drilled. Take the top *n*, then
find real words containing them, falling back to generated sequences.

### 8.5 Word-pool generation

Tokenise the source text, build a frequency table, drop tokens below a length threshold, then
sample with replacement weighted by frequency from a seeded PRNG. Optionally preserve
punctuation and capitalisation ratios so generated text feels like the source. The seed is
persisted with the session for reproducibility.
