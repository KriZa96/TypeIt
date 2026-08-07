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

---

## TI-108 — Export from the UI

**Type** feat · **Size** XS · **Priority** P3 · **Depends on** TI-077, TI-103

**Unit tests**
- Export writes to the chosen path and reports success or failure visibly.
- An unwritable path produces a clear message rather than silent failure.
- Exported content matches the CLI export byte for byte.

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
- [ ] The budget is met and enforced by a test that fails if it regresses.
