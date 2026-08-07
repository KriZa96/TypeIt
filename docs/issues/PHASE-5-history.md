# Phase 5 — History and analytics

**Milestone:** `v2.0.0-alpha.6` · **Issues:** TI-099 – TI-109 · **Goal:** make the recorded
history visible and useful.

Every run has been recorded since Phase 4. This phase turns that data into something a person
can learn from. Widget tests are snapshot-based; the aggregation work is tested against
synthetic histories of known shape.

---

## TI-099 — `Sparkline` widget

**Type** feat · **Size** S · **Priority** P1 · **Depends on** TI-095 · **Docs** [UX §5](../UX.md#5-glyph-sets)

Compact single-line trend using `▁▂▃▄▅▆▇█`, with an ASCII fallback of `.:-=+*%@`.

**Unit tests** (`SparklineTest.cpp`)
- N values render to N cells.
- All-equal values render as a flat mid-level line, not as all-minimum or a division by zero.
- A single value renders one cell.
- Zero values renders empty, not a crash.
- Negative values are rejected or clamped per the documented rule.
- More values than available width downsamples deterministically.
- ASCII fallback emits no byte above 0x7F.
- Snapshot for a known series.

**Acceptance**
- [x] Every degenerate series has a documented answer rather than an accident, and the answer
      is in the header beside the function: empty draws **nothing** (not a row of the lowest
      level, which would read as a run of zeroes); all-equal draws the **middle** level (not
      the floor, which would say a flat week was a bad one); negatives are clamped rather than
      rejected, because losing a month of history over one bad point is the worse failure.
- [x] Downsampling averages equal buckets rather than sampling every nth. A stride that landed
      on the troughs would draw a flat line over a clear trend, and there is a test that a
      rising series still reads as rising after being squeezed into ten cells.
- [x] Lives in `Charts.cpp` with the histogram and the line chart rather than in a file of its
      own. None of the three holds anything between frames, so each is a function over its
      data, and three headers that always change together are three places to forget one.

---

## TI-100 — `LineChart` widget

**Type** feat · **Size** M · **Priority** P1 · **Depends on** TI-095 · **Docs** [UX §3.4](../UX.md#34-results)

Axis-labelled chart for per-second WPM, with error markers overlaid.

**Unit tests** (`LineChartTest.cpp`)
- Axis ticks are chosen at readable intervals for ranges 0–10, 0–100, 0–1000.
- Y range adapts to the data with sensible padding; a flat series still renders.
- Error markers (`×`) land at the correct x position for their timestamp.
- An overlay series (the race pacer curve) renders distinctly from the primary series.
- Empty data renders an empty chart with axes and a message, not a crash.
- Very narrow widths degrade gracefully rather than overflowing.
- Snapshots at 80 and 120 columns.

**Acceptance**
- [x] Tick selection is `axis_ticks`, a free function over two doubles, and the test asserts on
      the vector rather than on the picture. Steps are 1, 2 or 5 times a power of ten: a step
      of 3.7 is evenly spaced and unreadable, and "readable" is the requirement.
- [x] A flat series gets one tick at the value it held, not an empty gutter — an axis with no
      numbers on it reads as a rendering bug rather than as a steady run.
- [x] Error markers sit **on** the line, in the series' own row, rather than in a strip along
      the bottom. A strip is a second chart sharing an axis, and matching its peaks to the
      line by eye is exactly the work the marker was meant to save.
- [x] Each row is drawn as one element per run of identical marks, not one per row, so the
      pacer overlay and the error markers keep their own colours. At `Mono` the glyph carries
      it, which is why they are different glyphs and not only different colours.
- [x] A width narrower than the label gutter drops the labels and keeps the plot, and nothing
      is written outside the box — asserted by measuring the longest rendered line.

---

## TI-101 — `Histogram` widget

**Type** feat · **Size** S · **Priority** P2 · **Depends on** TI-095

Distribution of WPM across runs.

**Unit tests** (`HistogramTest.cpp`)
- Bucketing is correct for known inputs; boundary values land in exactly one bucket.
- Bucket count adapts to the available width.
- A single data point renders one bar.
- Empty input renders empty.
- Bar heights scale to the tallest bucket.

**Acceptance**
- [x] Bucketing is asserted on `bucket_counts`, which returns a vector, rather than by reading
      block characters back out of a picture. Buckets are half-open upwards with the top one
      closed, so a boundary value lands in exactly one bucket and the maximum is not silently
      dropped off the end — both are tests.
- [x] The bucket count is derived from the width rather than asked for. A histogram with more
      buckets than columns cannot draw them, and one with far fewer wastes the box it was
      given.
- [x] Every value identical is one bucket rather than a division by zero.

---

## TI-102 — `Heatmap` widget

**Type** feat · **Size** M · **Priority** P1 · **Depends on** TI-095 · **Docs** [UX §3.5](../UX.md#35-history)

Keyboard layout coloured by error rate — the payoff for recording per-key statistics, turning
thousands of keystrokes into one glance that says which fingers to work on.

**Unit tests** (`HeatmapTest.cpp`)
- QWERTY layout renders in the correct physical arrangement.
- Error-rate → intensity mapping is monotonic.
- Keys with no data render as "no data", visibly distinct from "zero errors" — conflating those
  two would be actively misleading.
- Works at all four colour depths; in `Mono` the intensity is encoded by glyph density.
- Non-letter keys (punctuation, space) are included.
- Snapshot for a known stat set.

**Acceptance**
- [x] "No data" and "no errors" are drawn differently, and it is the test this widget exists
      for. A key never pressed and a key never missed are opposite facts; colouring both of
      them clean would send somebody off to practise everything except what they get wrong.
      `error_rate` returns `std::optional<double>` rather than a sentinel, because every
      sentinel here is a real value — zero is the *best possible* rate.
- [x] Five bands rather than a continuous scale. A typist acts on "this key is bad", not on the
      third decimal of its rate, and five colours are five decisions.
- [x] The intensity glyph is drawn at every depth, not only at `Mono`. A reader who cannot
      distinguish the colours is in the same position as a terminal that has none, and the ramp
      costs one character either way. Monotonicity is asserted over both glyph sets.
- [x] Digits and punctuation are in the layout, not only letters — a typist who misses every
      comma learns nothing from a picture without one, and the stats have the data regardless.

---

## TI-103 — `HistoryScreen`

**Type** feat · **Size** L · **Priority** P1 · **Depends on** TI-099 – TI-102, TI-069 · **Docs** [UX §3.5](../UX.md#35-history)

Trend chart, filters, personal bests, distribution, streaks, totals, heatmap, and a session
list.

**Unit tests** (`HistoryScreenTest.cpp`)
- Mode and date-range filters change the displayed data and are combinable.
- Empty history renders a helpful empty state, not a blank screen or a crash.
- Session list paginates and scrolls; selecting a row opens `SessionDetailScreen`.
- Personal bests display per `(mode, parameter)` — a 15 s best and a 60 s best are separate
  records, because they measure different things.
- Totals and streak figures match `HistoryService` for a known history.
- Snapshots at 80×24 and 120×40.

**Acceptance**
- [x] **Nothing is queried in `render`.** The database is asked when the screen opens and when
      a filter changes, and drawing reads a member. `HistoryScreenTest.NothingIsQueriedByDrawing`
      counts the calls and asserts drawing twice adds none — a query inside a render callback
      runs sixty times a second against a file on disk, which is 1.0's `std::stoi`-in-a-render-
      transform wearing a much more expensive hat.
- [x] A failing query is reported **in place**, and the parts that did read still draw. A
      screen that blanks because one aggregate failed tells its reader nothing about the runs
      it can see.
- [x] The two filters combine, because the range touches `since` and the mode touches `mode`
      and neither reaches for the other. `all` is the *absence* of a mode filter, not a mode
      called "all" — a query for `mode = 'all'` matches nothing.
- [x] Abandoned runs are listed. This screen is a record of what happened, and a run left half
      way through happened; `--stats` excludes them because it is about records, which is a
      different question.
- [x] Empty history renders a sentence, not a table of zeros — a screen of zeroes reads as
      broken rather than as new. It renders with no history *source* at all too, which is what
      a layout test and a build with no database both look like.
- [x] Selecting a row **reports** the session rather than pushing a screen. TI-081's rule: the
      stack is driven from one place. Nothing consumes `take_opened()` yet — the screen it
      opens is TI-104, which depends on this issue by design, and which needs a repository
      method to read one session whole. Pressing Enter is therefore inert until then, and that
      is deliberate rather than missed.
- [x] Snapshots at both sizes.

---

## TI-104 — `SessionDetailScreen`

**Type** feat · **Size** S · **Priority** P2 · **Depends on** TI-103

A single past run in full.

**Unit tests** (`SessionDetailScreenTest.cpp`)
- All stored metrics render, including the timeline chart.
- A session whose text has since been deleted (`text_id` NULL) renders without a crash.
- A session recorded by an older `app_version` is displayed with a note, since metric
  definitions may differ across a MAJOR version
  ([VERSIONING §9](../VERSIONING.md#9-compatibility-promises)).
- Back returns to the history screen with filters and scroll position preserved.

**Acceptance**
- [x] Needed a port method, and got one: `IHistoryRepository::session(id)` returns a whole
      `SessionRecord` including its per-second samples, which `SessionRow` deliberately omits.
      Separate from `query` rather than a flag on it because the two have opposite shapes — a
      list wants many rows and no samples, a detail view wants one row and all of them, and a
      `query` that returned samples would load three hundred thousand of them to draw a list of
      forty. Both implementations are held to it by the contract suite.
- [x] `ErrorCode::SessionNotFound` is a new code rather than a reused `DbQuery`. The database
      answered; the answer was "no such run". A stale bookmark is a user error, not a fault,
      and the two deserve different messages.
- [x] `keystrokes` is **not** stored — the schema keeps time, speed and errors, which is what
      the chart draws — so a sample read back is not the one that was written. The contract
      test asserts that rather than leaving somebody to find it in a diff, and the fake zeroes
      the field too: a fake that returned more than the real adapter would be a fake that lies
      about the port.
- [x] A run whose text has since been deleted renders. `text_id` is absent for generated text
      and for a library entry somebody removed; neither is an error, and NULL must not read
      back as text zero.
- [x] A run from an older MAJOR version is shown with a note, because 2.0 changed what WPM and
      accuracy *mean* (VERSIONING §9) and showing the two side by side without saying so is the
      quiet kind of wrong. A version that cannot be parsed, or is missing entirely, counts as
      **not** this one: "unknown" is not "current".
- [x] Back pops rather than replaces, so the history underneath keeps its filters and its
      scroll position — which is the whole reason the stack is a stack.

---

## TI-105 — `ResultsScreen` enrichment

**Type** feat · **Size** M · **Priority** P1 · **Depends on** TI-100, TI-093 · **Docs** [UX §3.4](../UX.md#34-results)

Upgrade the parity version with the per-second chart, error markers, comparison against average
and personal best, worst error pairs, and slowest bigrams.

**Unit tests** (`ResultsScreenTest.cpp`, extended)
- The chart matches the session timeline.
- Comparison lines are correct, and are **absent rather than wrong** when there is no history to
  compare against.
- A new personal best is announced distinctly.
- Worst pairs and slowest bigrams show the top five, with fewer shown gracefully when data is
  sparse.
- Multi-byte graphemes display correctly in the pair list.

**Acceptance**
- [x] The pair and bigram lists come from what `finish` already computed, not from a second
      pass: `SessionResult` now carries the `KeyStats` and `ErrorMap` it built in order to
      write them. A results screen that derived its own would be a second implementation of the
      metrics with nothing keeping the two in step — and by the time it drew, the session it
      would have derived them from is gone.
- [x] Comparisons are **absent rather than wrong** when there is nothing to compare against. A
      line reading "+0 vs average" on somebody's first run invents a baseline out of the run
      itself. One session in the history *is* this one, so the comparison needs two.
- [x] A new personal best is announced on its own line. The first completed run counts as one
      by definition, which is the point rather than an edge case.
- [x] An unmeasured bigram is not a slow one. A pair with no latency samples — two keys typed
      once, with the interval thrown out as a pause — has no mean to be slowest by, so it is
      left out rather than sorted as zero.
- [x] Ties break on the key, so two runs with the same mistakes list them in the same order. A
      list that reshuffled between draws would be unreadable, and `std::map` iteration order is
      not a promise about ties in the counts.
- [x] Multi-byte graphemes pass through unchanged — the pair list is the likeliest place for a
      `ć` to appear, because it is the one people miss, and reading one byte of it is 1.0's
      original defect.
- [x] The record-only constructor stays. A run finished before there was any history to compare
      it against is a normal thing to have, and it is what the parity snapshot draws.

---

## TI-106 — Menu recent-runs sparkline

**Type** feat · **Size** XS · **Priority** P2 · **Depends on** TI-099, TI-091

The last ten runs, plus average, best, and accuracy — on the menu, because the point of
recording history is to see it without asking.

**Unit tests**
- Fewer than ten runs renders correctly.
- Zero runs renders a neutral prompt, not an empty box.
- Figures match `HistoryService`.

**Acceptance**
- [x] Read once when the menu opens, never in `render` — same guard as the history screen, and
      the same test: drawing twice asks the database nothing.
- [x] Oldest first. `query` returns newest first, and a trend line drawn in that order runs
      backwards, which is a picture that says the opposite of the truth.
- [x] A failing query leaves the menu usable. A run nobody can start because a sparkline failed
      to load would be the worse trade, so the failure is swallowed here rather than reported —
      the one place in the rebuild where that is the right answer, because the menu's job is
      not to show history.

---

## TI-107 — Streaks and daily goals

**Type** feat · **Size** S · **Priority** P2 · **Depends on** TI-069

**Unit tests** (`StreakTest.cpp`)
- Consecutive days increment; a gap resets.
- Two sessions in one day count as one day.
- The day boundary uses local time, documented, and is tested across a DST transition.
- Goal met by time and goal met by run count both work.
- Longest streak is retained after the current streak breaks.
- A session started before midnight and finished after it is attributed per the documented
  rule.

**Acceptance**
- [x] The streak now counts days that **met the goal**, not days with a run — which is what
      GAMEPLAY §7.4 says and what the implementation did not do. Two days of real practice with
      a token thirty-second day between them was being reported as a streak of three, and a
      number that counts turning up is a number worth nothing.
- [x] **Either bar clears the day.** Ten minutes of practice and five quick runs are both a day
      somebody showed up; requiring both would make the streak a chore rather than a record.
      Both are tested alone, and so is neither.
- [x] Zero on both bars means any run counts, for somebody who has turned the goal off.
      `DailyGoal::any()` is the same thing spelled for a caller with no configuration in
      hand — a test, or a build without one — so nobody gets a zero by accident.
- [x] The rule for a run that crosses midnight is **the day it started in**, documented on
      `streak` and tested. The alternative attributes a run to a day its typist may never have
      been awake for, and `started_at` is the field the list is ordered by regardless.
- [x] A DST shift does not break a streak. Days are counted from an offset the caller supplies,
      so a transition is a change of offset; two runs a calendar day apart stay a two-day
      streak read from either side of it. The companion test is the other half: a run at 23:00
      UTC is today in UTC and tomorrow an hour east, and the day it lands in follows the offset
      rather than being fixed at import.
- [x] `[goals]` in `config.toml`, with `daily_minutes` and `daily_runs`, so the goal is
      configurable as GAMEPLAY asks.
- [x] `--stats` and the history screen read the **same** goal. Config loading moved to one
      helper in the composition root for exactly that reason: two paths that each loaded their
      own would eventually disagree about what a streak means, and the two numbers sit side by
      side in a bug report.
- [x] Today's progress is shown either way — "goal met" alone leaves somebody wondering whether
      the line failed to draw or the day did.

---

## TI-108 — Export from the UI

**Type** feat · **Size** XS · **Priority** P3 · **Depends on** TI-077, TI-103

**Unit tests**
- Export writes to the chosen path and reports success or failure visibly.
- An unwritable path produces a clear message rather than silent failure.
- Exported content matches the CLI export byte for byte.

**Acceptance**
- [x] `ctrl-e` opens a prompt on the history screen; the prompt owns the keyboard while it is
      open, so a path containing a `j` does not cycle a filter behind it.
- [x] The extension chooses the format — `.json` for JSON, anything else CSV. Predictable, and
      it reaches both without a second control nobody would find.
- [x] The bytes come from `HistoryService`, the same call `--export` makes, and the test asserts
      equality with it rather than re-describing the format. A second formatter here would drift
      from the CLI's inside a release.
- [x] The **filter on screen** is the filter exported. A filtered view that wrote the whole
      history would be a quiet surprise in somebody's spreadsheet.
- [x] Writing goes through a `TextWriter` the composition root supplies, like `TextLoader`
      before it: this is the only way anything in `tui` puts a file on a disk, and it does it
      through something handed to it. With no writer the screen says exporting is unavailable
      rather than offering a key that does nothing.
- [x] An unwritable path is named, not swallowed — it is the common failure and the one a
      silent export hides completely. An empty path leaves the prompt open to be corrected.

---

## TI-109 — History performance

**Type** perf · **Size** M · **Priority** P1 · **Depends on** TI-103 · **Docs** [ARCHITECTURE §6.5](../ARCHITECTURE.md#65-performance-budget)

**Scope**
- In: push aggregation into SQL rather than loading rows and summing in C++; add indexes where
  the query plan needs them; a benchmark test with a synthetic 10,000-session database.
- Out: —

**Unit tests** (`HistoryPerfTest.cpp`)
- History screen data loads in < 200 ms with 10,000 sessions and 300,000 samples.
- Trend aggregation is performed by SQL — asserted by inspecting `EXPLAIN QUERY PLAN` for an
  index scan rather than a full table scan.
- Memory use during load is bounded and does not scale with total history.
- A database seeded to 10k sessions is generated by a fixture, not committed.

**Acceptance**
- [x] The budget is met and enforced by a test that fails if it regresses: ten thousand
      sessions and three hundred thousand samples, seeded by the fixture and never committed,
      loaded well inside the 200 ms of ARCHITECTURE §6.5.
- [x] Aggregation moved into SQL. `IHistoryRepository::daily_totals` returns one row per local
      day, and the trend, the streak and the daily goal all read it — where each of them used
      to load every row and group in C++. A year is at most 366 buckets however many runs are
      behind them, so the memory a history screen needs stopped growing with how much somebody
      has typed. `TheTrendDoesNotGrowWithTheHistoryBehindIt` asserts that as a count rather
      than as bytes, which is the part a test can actually hold.
- [x] An index where the plan needed one. `EXPLAIN QUERY PLAN` reported `SCAN session`: the
      existing index covers the ordering but not the columns, so SQLite read every twenty-three
      column row — two of them free text — to sum three numbers. `002_daily_totals_index.sql`
      adds a covering index and the plan becomes a covering-index scan.
- [x] The plan test explains **the statement the repository runs**, reached through
      `daily_totals_sql()`. Its first draft planned a hand-copied simplification, reported a
      table scan the real query does not do, and would have gone on passing after the real one
      regressed — which is the failure mode this kind of test exists to avoid and the one it is
      easiest to write.
- [x] The fixture is asserted to have really seeded ten thousand rows. A budget met against an
      empty database is a budget met against nothing, and a seeding bug looks exactly like a
      fast load — the first version of the seed did fail (`save` opens its own transaction and
      SQLite does not nest them) and every timing assertion passed.
