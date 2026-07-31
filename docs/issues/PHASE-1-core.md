# Phase 1 — `typeit::core`

**Milestone:** `v2.0.0-alpha.2` · **Issues:** TI-023 – TI-052 · **Goal:** the complete domain,
headless, with zero third-party dependencies.

This is the largest phase and the one where test coverage matters most, because everything
here is pure: there is no terminal, no database, and no clock to stand up, so there is no
excuse for an untested branch. The coverage gate is **95% on `metrics` and `text`, 90% on
`core` overall** (TI-052).

The legacy application is untouched throughout this phase and keeps passing its own tests.

---

## TI-023 — Scaffold `typeit_core` with dependency isolation

**Type** build · **Size** S · **Priority** P0 · **Depends on** TI-012 · **Docs** [BUILD §3](../BUILD.md#3-project-structure), [ADR-001](../ARCHITECTURE.md#adr-001--typeitcore-has-zero-third-party-dependencies)

Create `libs/core` with a `PUBLIC` include directory and **nothing on
`target_link_libraries`**. That absence is the enforcement mechanism: FTXUI's include
directories are not visible to this target, so `#include <ftxui/…>` inside `core` is a compile
error rather than a review comment.

**Scope**
- In: `typeit_core` static library + `typeit::core` alias; `tests/core` test target;
  `libs/core/include/typeit/core/` layout; warnings applied.
- Out: any domain code.

**Tests** — a placeholder test proving the target builds and links, plus a **negative build
test**: a source file that includes `<ftxui/dom/elements.hpp>`, compiled via
`try_compile`, asserting it *fails*. Without that, the dependency rule is enforced by
discipline alone.

**Acceptance**
- [ ] `typeit_core` links only the standard library.
- [ ] The negative build test fails to compile, as expected.
- [ ] Includes are target-qualified (`typeit/core/…`), never relative.

---

## TI-024 — `util/Units.h` — strong domain types

**Type** feat · **Size** S · **Priority** P0 · **Depends on** TI-023 · **Docs** [TECHNICAL §1.1](../TECHNICAL.md#11-units-coreutilunitsh), [ADR-010](../ARCHITECTURE.md#adr-010--strong-types-for-domain-quantities)

`Wpm`, `Accuracy`, `Millis`, `GraphemeIndex`, `SessionId`, `TextId`. Each a single-member
struct with explicit construction and only the arithmetic that makes sense for it. This is what
makes [defect C7](../CODEBASE_REVIEW.md#4-correctness-defects) — `size_t` compared against
`int`, where an empty text turns `size() - 1` into `SIZE_MAX` — unwritable.

**Scope**
- In: the six types; comparison operators; the arithmetic each one legitimately supports
  (`Millis` subtracts to `Millis`; `GraphemeIndex` increments; `Accuracy` does not add).
- Out: formatting (belongs at the presentation boundary).

**Unit tests** (`UnitsTest.cpp`)
- Construction is explicit: `Wpm w = 5.0;` does not compile (`static_assert` on
  `!std::is_convertible_v`).
- Types do not interconvert: `Wpm` from `Accuracy` does not compile.
- Comparison is total and consistent for each type.
- `Millis` subtraction yields `Millis`; ordering is correct across negative differences.
- `GraphemeIndex` increment/decrement at 0 behaves as documented.
- All six are trivially copyable and no larger than their payload.

**Acceptance**
- [ ] Zero runtime overhead versus the raw type (`static_assert` on `sizeof`).
- [ ] Illegal conversions are compile errors, asserted via `static_assert`.

---

## TI-025 — `util/Result.h` — error type and `std::expected` aliases

**Type** feat · **Size** S · **Priority** P0 · **Depends on** TI-023 · **Docs** [TECHNICAL §1.2](../TECHNICAL.md#12-result-coreutilresulth), [ADR-009](../ARCHITECTURE.md#adr-009--stdexpected-for-expected-failures-exceptions-for-broken-invariants)

`ErrorCode` enum, `Error` struct (code + user-facing message + context), `Result<T>` and
`Status` aliases over `std::expected`.

**Scope**
- In: the types; helper constructors for common errors; a formatting function for display.
- Out: logging (Phase 2).

**Unit tests** (`ResultTest.cpp`)
- A `Result` carrying a value converts to `true`; carrying an error, to `false`.
- Error context is preserved through moves and through propagation across three call levels.
- `Status` composes: a failing inner call short-circuits the outer.
- `[[nodiscard]]` is honoured — a discarded `Result` warns (verified by a `try_compile` with
  `-Werror`).
- Every `ErrorCode` has a non-empty default message (table-driven over the enum).

**Acceptance**
- [ ] Compiles on gcc, clang, and MSVC at the documented floor.
- [ ] No exception is thrown by any function returning `Result`.

---

## TI-026 — `util/IClock.h` and `FakeClock`

**Type** feat · **Size** XS · **Priority** P0 · **Depends on** TI-024 · **Docs** [TECHNICAL §1.3](../TECHNICAL.md#13-iclock-coreutiliclockh)

The time port. This single interface is what removes every `sleep_for` from the test suite —
the current `TimerTest` sleeps 1–2 seconds per test and asserts the exact string `"9s"` after a
one-second sleep, which is a race against the scheduler.

**Scope**
- In: `IClock` with `now() -> Millis`; `FakeClock` in the test support library with
  `set(Millis)` and `advance(Millis)`.
- Out: `SystemClock` (Phase 2 — it is an adapter).

**Unit tests** (`FakeClockTest.cpp`)
- `advance` accumulates; `set` replaces.
- Time never moves without an explicit call.
- Two clocks are independent.

**Acceptance**
- [ ] `FakeClock` lives in test support, is usable by every later phase.
- [ ] No `core` code calls `std::chrono::steady_clock` directly (`git grep` clean).

---

## TI-027 — `Grapheme` and the UTF-8 decoder

**Type** feat · **Size** M · **Priority** P0 · **Depends on** TI-025 · **Docs** [TECHNICAL §1.4](../TECHNICAL.md#14-text-coretext), [ADR-004](../ARCHITECTURE.md#adr-004--text-is-a-sequence-of-grapheme-clusters-not-bytes)

`Grapheme` — up to 12 inline UTF-8 bytes plus length and display width, trivially copyable, no
allocation — and a UTF-8 → code point decoder that reports errors rather than producing
replacement characters silently.

**Scope**
- In: the type; decoding with position-accurate error reporting; `view()` and equality.
- Out: cluster joining (TI-028), width tables (TI-029).

**Unit tests** (`Utf8DecoderTest.cpp`, `GraphemeTest.cpp`) — table-driven
- 1/2/3/4-byte sequences decode to the right code points.
- Overlong encodings are rejected.
- Surrogate code points (U+D800–U+DFFF) are rejected.
- Truncated sequence at end of input is rejected with the correct byte offset.
- Invalid continuation byte is rejected with the correct byte offset.
- `Grapheme` equality compares bytes, not pointers.
- `sizeof(Grapheme) <= 16` and `std::is_trivially_copyable_v<Grapheme>`.

**Acceptance**
- [ ] Every rejection names the byte offset.
- [ ] No allocation in the decode path (verified with a counting allocator).

---

## TI-028 — `Segmenter` — grapheme cluster breaking

**Type** feat · **Size** L · **Priority** P0 · **Depends on** TI-027 · **Docs** [ARCHITECTURE §6.2](../ARCHITECTURE.md#62-unicode-scope)

Join code points into user-perceived characters. **Scope boundary is explicit and documented:**
combining marks, ZWJ sequences, regional indicator pairs, variation selectors, and CRLF. Full
UAX #29 with complete break-property tables, bidi, and complex-script shaping are out of scope.

This is the issue that makes "type any text you want" actually true, and it retires the
README's claim that FTXUI cannot handle ćčšđž — the byte-per-`char` model was substantially the
cause.

**Scope**
- In: the break rules above; a `segment()` function over decoded code points.
- Out: width (TI-029); normalisation (TI-071).

**Unit tests** (`SegmenterTest.cpp`) — table-driven, this is the highest-risk code in the phase
- `"hello"` → 5 clusters.
- `"čšž"` → 3 clusters, 6 bytes.
- `"é"` as U+00E9 → 1 cluster; as `e` + U+0301 → 1 cluster; both compare unequal (that is
  normalisation's job, not segmentation's).
- `"👨‍👩‍👧"` (ZWJ family) → 1 cluster.
- `"🇭🇷"` (two regional indicators) → 1 cluster; `"🇭🇷🇩🇪"` → 2 clusters; a lone regional
  indicator → 1 cluster.
- Emoji + variation selector → 1 cluster.
- `"a\r\nb"` → 3 clusters (CRLF joined); `"a\n\rb"` → 4.
- A cluster longer than 12 bytes truncates at the documented boundary and reports it.
- Empty input → 0 clusters.
- Round trip: concatenating every cluster's bytes reproduces the input exactly.

**Acceptance**
- [ ] Every table row passes.
- [ ] The round-trip property holds for a corpus of ~1000 mixed-script lines.
- [ ] Out-of-scope cases are documented in the header, not silently mishandled.

---

## TI-029 — Display width tables

**Type** feat · **Size** M · **Priority** P0 · **Depends on** TI-028

Width 2 for East Asian Wide/Fullwidth and emoji presentation, 0 for combining marks, 1
otherwise. Tables are **generated** from Unicode data files by a script committed alongside
them — hand-maintained ranges rot.

**Scope**
- In: the generator script; the generated header; `width(codepoint)` and `Grapheme::width`.
- Out: terminal-specific width quirks (some emulators disagree about emoji width; documented,
  and a config override lands in Phase 4).

**Unit tests** (`WidthTest.cpp`)
- ASCII → 1. `"日本語"` → 2 each, 6 total. Hangul → 2. Fullwidth forms → 2.
- Combining acute → 0; `e` + acute as one cluster → total 1.
- Emoji → 2; emoji + VS15 (text presentation) → 1.
- Control characters → 0.
- The generator is deterministic: regenerating produces a byte-identical file.

**Acceptance**
- [ ] Generated header matches a regeneration run in CI.
- [ ] Generator documents which Unicode version it consumed.

---

## TI-030 — `TextBuffer`

**Type** feat · **Size** M · **Priority** P0 · **Depends on** TI-029 · **Docs** [TECHNICAL §1.4](../TECHNICAL.md#14-text-coretext)

The domain's text type: `from_utf8` factory returning `Result`, grapheme indexing, slicing,
word count, total display width. One contiguous allocation.

**Scope**
- In: the type and its accessors.
- Out: wrapping (TI-031).

**Unit tests** (`TextBufferTest.cpp`)
- `from_utf8` rejects invalid input with the byte offset.
- Empty text → `size() == 0`, valid buffer, not an error.
- `at()` on an out-of-range index is a precondition violation (assert in debug), and
  `at_checked()` returns `Result`.
- `to_string(from, to)` round-trips; `from == to` yields empty; reversed range is rejected.
- **Word count**: port all eight cases from the existing `test_word_count.cpp` — empty, one
  word, leading/trailing/surrounding spaces, two words, double spaces, newline-separated,
  multiple spaces around and between.
- Word count with non-breaking space, tab, and CJK text (no spaces).

**Acceptance**
- [ ] All eight legacy word-count cases pass with identical expectations.
- [ ] One allocation per buffer (counting allocator).

---

## TI-031 — `Wrapper` — pure line wrapping

**Type** feat · **Size** M · **Priority** P0 · **Depends on** TI-030 · **Docs** [TECHNICAL §1.4](../TECHNICAL.md#14-text-coretext)

`wrap(span<const Grapheme>, columns) -> LineBreaks`. A **pure function**, deliberately not
state baked into the buffer — that is what makes terminal resize a matter of calling it again
rather than rebuilding the session, fixing the limitation in
[review §3.3](../CODEBASE_REVIEW.md#33-ftxui-types-live-in-the-domain).

**Scope**
- In: greedy wrapping at the last space that fits, accounting for display width; hard break for
  a word longer than the line.
- Out: justification, hyphenation.

**Unit tests** (`WrapperTest.cpp`) — table-driven
- Text shorter than the width → one line.
- Exact-fit line → one line, no trailing empty line.
- Break at the last space before the limit.
- A single word longer than the width → hard-broken at exactly `columns`.
- Wide characters straddling the boundary do not overflow the column budget.
- Trailing spaces at a break are absorbed, not carried to the next line.
- `columns == 1` produces one grapheme per line; `columns == 0` is rejected.
- Empty text → zero lines.
- **Legacy behaviour ported from `test_text.cpp`**: newline forces a break; `"Hello\nNo\nHello"`
  → 3 lines; `"\n\n"` → 2; `"\n\nA"` → 3; the three "Lorem ipsum" cases producing 1, 2, and 3
  lines at the legacy 55-column threshold.
- Property: concatenating all lines reproduces the input.
- Property: no line exceeds `columns` in display width.

**Acceptance**
- [ ] All legacy wrapping expectations reproduce exactly at width 55.
- [ ] Both properties hold over a randomised corpus.
- [ ] The function is `const`-correct and allocation-bounded.

---

## TI-032 — `Keystroke` and `KeystrokeLog`

**Type** feat · **Size** S · **Priority** P0 · **Depends on** TI-030 · **Docs** [ADR-002](../ARCHITECTURE.md#adr-002--the-keystroke-log-is-the-single-source-of-truth-all-metrics-are-derived)

The append-only event log that is the source of truth for everything measured.

**Scope**
- In: `Keystroke {at, kind, typed, target}`; `KeystrokeLog` with `append`, `events`, `size`,
  `duration`. Append-only — no mutation, no rewind, no erase.
- Out: serialisation (Phase 2).

**Unit tests** (`KeystrokeLogTest.cpp`)
- Appends preserve order; `events()` is a stable view.
- `duration()` on an empty log is zero, not negative or UB.
- `duration()` is last minus first timestamp.
- Reserve/growth does not invalidate previously observed values.
- The API exposes no way to mutate or remove an event (`static_assert` on the absence of
  non-const accessors).

**Acceptance**
- [ ] No public mutating operation other than `append`.
- [ ] 100k appends stay within the documented memory bound (~16 bytes/event).

---

## TI-033 — `TypingModel` state machine

**Type** feat · **Size** L · **Priority** P0 · **Depends on** TI-032 · **Docs** [GAMEPLAY §6](../GAMEPLAY.md#6-typing-rules), [TECHNICAL §1.6](../TECHNICAL.md#16-the-typing-model)

Cursor plus per-grapheme state (`Pending`/`Correct`/`Incorrect`/`Corrected`/`Missed`), driven
by `type()` and `backspace()`. **Time arrives as a parameter**; the model never reads a clock,
never touches I/O, and is single-threaded by contract.

`Corrected` being distinct from `Correct` is what lets accuracy and final correctness be
reported separately without ambiguity.

**Scope**
- In: the state machine with default rules.
- Out: rule variants (TI-034).

**Unit tests** (`TypingModelTest.cpp`) — the behavioural heart of the phase
- Correct grapheme → `Correct`, cursor advances, event logged.
- Wrong grapheme → `Incorrect`, cursor advances.
- Backspace over `Incorrect` → back to `Pending`, cursor retreats.
- Backspace then correct retype → `Corrected`, **not** `Correct`.
- **Backspace at index 0 is a no-op** and never underflows — the boundary the legacy engine
  survives only by an unrelated guard.
- Typing past the end does not advance the cursor and logs nothing.
- **Empty target text → immediately finished, no out-of-bounds access.** The legacy code
  survives this case only because `should_finish_game()` happens to return early; here it is
  asserted behaviour.
- Space skipping mid-word marks intervening positions `Missed`.
- Backspace across a word boundary restores the previous word's states.
- Every event appended carries the correct `target` index.
- Property: after any sequence of operations, `cursor <= text.size()`.
- Property: states and text have equal length, always.
- **Ported from `test_input_line_engine.cpp`**: transition on space at a line boundary,
  backspace at line start returning to the previous line, no-op on empty input, removal of the
  first element, and the "type more than the text" case.

**Acceptance**
- [ ] Every transition in the GAMEPLAY §6 diagram has at least one test.
- [ ] Both invariant properties hold under a randomised operation fuzz of 10k sequences.
- [ ] The five ported legacy behaviours reproduce.

---

## TI-034 — `TypingRules` variants

**Type** feat · **Size** M · **Priority** P1 · **Depends on** TI-033 · **Docs** [GAMEPLAY §6](../GAMEPLAY.md#6-typing-rules)

`stop_on_error` (off/letter/word), `allow_backspace`, `strict_spaces`, `space_advances_word`,
`blind_mode`, `confidence_mode` (off/on/max).

**Scope**
- In: rule application inside `TypingModel`.
- Out: UI exposure (Phase 4); `blind_mode` is a model flag here and a rendering decision there.

**Unit tests** (`TypingRulesTest.cpp`) — one fixture per rule
- `stop_on_error = letter`: input blocked until the error is corrected; the blocked keystroke is
  logged as an attempt or not, per the documented decision.
- `stop_on_error = word`: blocked at the word boundary, not the letter.
- `allow_backspace = false`: backspace is a no-op and logs nothing.
- `strict_spaces = true` vs `false`: a missing space is an error vs absorbed.
- `space_advances_word`: space mid-word jumps to the next word start and marks the skipped
  graphemes `Missed`.
- `confidence_mode = on`: backspace across a completed word is refused.
- `confidence_mode = max`: backspace is refused entirely.
- Matrix test: every rule combination applied to one fixed input produces the documented final
  state (table-driven, generated).

**Acceptance**
- [ ] Every rule has isolated tests plus a place in the combination matrix.
- [ ] Defaults match [TECHNICAL §6](../TECHNICAL.md#6-configuration-file).

---

## TI-035 — `LogBuilder` test DSL

**Type** test · **Size** S · **Priority** P0 · **Depends on** TI-032 · **Docs** [TESTING §3.4](../TESTING.md#34-metrics--property-tests)

A fluent builder for synthetic keystroke logs, so every metric test is exact, instant, and free
of timing dependence:

```cpp
auto log = LogBuilder{}.type("h", 100ms).type("q", 200ms)
                       .backspace(300ms).type("e", 400ms).build();
```

**Scope**
- In: the builder; helpers for common shapes — `perfect(text, wpm)`, `with_errors(text, n)`,
  `bursty(text)`, `with_pauses(text, gaps)`.
- Out: file-based scripts (TI-075).

**Unit tests** (`LogBuilderTest.cpp`)
- `perfect(text, 60_wpm)` produces a log whose computed gross WPM is 60 ± 0.5.
- Timestamps are monotonic.
- `with_errors(text, 3)` produces exactly three first-attempt errors.

**Acceptance**
- [ ] Used by every metric test in TI-036 – TI-042.
- [ ] Lives in test support, not in `core`.

---

## TI-036 — Metrics: speed (raw / gross / net)

**Type** feat · **Size** M · **Priority** P0 · **Depends on** TI-035 · **Docs** [GAMEPLAY §4.1](../GAMEPLAY.md#41-speed)

Replaces `WordCalculatorEngine` entirely. The legacy formula counts whitespace-delimited words
in whatever the user typed — including wrong ones — against an elapsed time that is **clamped**
to the configured duration, producing a number comparable to no other typing test
([defect C5](../CODEBASE_REVIEW.md#4-correctness-defects)).

**Scope**
- In: `raw_wpm`, `gross_wpm`, `net_wpm` on the standard five-graphemes-per-word convention;
  **unclamped** elapsed time.
- Out: rolling and peak (TI-039).

**Unit tests** (`MetricsSpeedTest.cpp`)
- 60 correct graphemes in 60 s → gross 12 WPM (60/5 per minute). Worked by hand in the test
  comment.
- 300 correct graphemes in 60 s → gross 60 WPM.
- Raw includes errors; gross excludes them; `net = gross − uncorrected/minute`.
- Property: `net_wpm <= gross_wpm <= raw_wpm`, always.
- Property: doubling every timestamp halves every WPM.
- Empty log → all zeros, no NaN, no division by zero.
- A single keystroke → finite result.
- Net floors at 0 and never goes negative, even with more errors than minutes.
- **Explicitly documented divergence** from `test_word_calculator.cpp`: those tests assert the
  legacy formula and their expected values change here. The commit message must say so.

**Acceptance**
- [ ] Three worked examples verified by hand against the GAMEPLAY §4.1 formulas.
- [ ] Both properties hold over randomised logs.
- [ ] No clamping of elapsed time anywhere in the computation.

---

## TI-037 — Metrics: accuracy and final correctness

**Type** feat · **Size** M · **Priority** P0 · **Depends on** TI-035 · **Docs** [GAMEPLAY §4.2](../GAMEPLAY.md#42-accuracy)

**This issue closes [defect C4](../CODEBASE_REVIEW.md#4-correctness-defects)**, the most
user-visible bug in the current application: `remove_element()` never pops from the accuracy
vector, so typing `x`, backspacing, and typing the correct `a` yields 50% accuracy on a
perfectly typed character — permanently, with no way to recover.

**Scope**
- In: accuracy (first-attempt correctness per target position) and final correctness
  (proportion of the finished text that matches), both derived from the log.
- Out: per-key breakdown (TI-041).

**Unit tests** (`MetricsAccuracyTest.cpp`) — the highest-value tests in the project
- **A perfectly typed run has accuracy 100% regardless of how many backspaces it contains.**
- Type wrong → backspace → type correct: exactly **one** attempt at that position, recorded as
  a first-attempt error. Not two samples.
- **Typing and deleting the same passage *n* times does not change the denominator**, for
  n = 1, 5, 50.
- Accuracy with 1 error in 100 graphemes → 99%.
- Final correctness is 100% after all errors are corrected, while accuracy is below 100%.
- Empty log → 0, not NaN.
- All-wrong input → accuracy 0, final correctness 0.
- Property: accuracy ∈ [0,1] and final correctness ∈ [0,1] for any log.
- Property: replaying the same log twice yields identical values.
- **Explicitly documented divergence** from `test_input_accuracy.cpp`, whose expectations
  encode the defective behaviour.

**Acceptance**
- [ ] The three backspace properties above pass — they are the definition of done for this
      issue.
- [ ] A regression test named for defect C4 exists and references it.

---

## TI-038 — Metrics: consistency

**Type** feat · **Size** S · **Priority** P1 · **Depends on** TI-036 · **Docs** [GAMEPLAY §4.3](../GAMEPLAY.md#43-consistency)

`100 × (1 − σ/μ)` over per-second gross WPM samples, floored at 0. Buckets with no keystrokes
count as 0 WPM — that is what makes the metric detect pauses rather than ignore them.

**Unit tests** (`MetricsConsistencyTest.cpp`)
- A perfectly even run → 100 (within float tolerance).
- A bursty run with **identical totals** to the even run → strictly lower. This is the test that
  proves the metric measures what it claims.
- A run with a 10-second pause scores lower than the same keystrokes without the pause.
- Single-bucket run → defined value, no division by zero.
- Empty log → 0.
- Property: result ∈ [0, 100] for any log.

**Acceptance**
- [ ] The even-vs-bursty comparison passes with identical totals.
- [ ] Matches the published Monkeytype definition on a hand-computed example.

---

## TI-039 — Metrics: rolling and peak-sustained WPM

**Type** feat · **Size** M · **Priority** P0 · **Depends on** TI-036 · **Docs** [TECHNICAL §8.1](../TECHNICAL.md#81-rolling-wpm-live-hud)

Sliding-window gross WPM with a maintained left index — **amortised O(1) per tick, never a
rescan of the whole log**. Required by the live HUD, by endless mode, and by race mode's
adaptive start speed.

**Unit tests** (`MetricsRollingTest.cpp`)
- Constant-rate typing → rolling WPM converges to the true rate.
- A rate step change is reflected within one window length.
- Window larger than the log → equals cumulative gross WPM.
- Idle period → rolling WPM decays to 0.
- `peak_sustained_wpm` ignores a 2-second burst but captures a 12-second plateau
  (sustain threshold 10 s).
- **Performance**: 100k events with per-tick rolling computation completes within the
  [§6.5 budget](../ARCHITECTURE.md#65-performance-budget); assert the left index advances
  monotonically and total work is linear.

**Acceptance**
- [ ] Complexity asserted, not assumed — an instrumented counter proves no rescan.
- [ ] Peak-sustained ignores bursts below the threshold.

---

## TI-040 — Metrics: timeline

**Type** feat · **Size** S · **Priority** P1 · **Depends on** TI-039 · **Docs** [TECHNICAL §5](../TECHNICAL.md#5-database-schema-v1)

Per-second samples of WPM and error count, for the results chart and the `session_sample`
table.

**Unit tests** (`TimelineTest.cpp`)
- A 30-second run at 1-second buckets → 30 samples.
- Bucket boundaries are half-open and consistent; a keystroke exactly on a boundary lands in
  exactly one bucket.
- Errors are attributed to the bucket containing the keystroke.
- Sub-bucket run → one sample.
- Empty log → zero samples.
- Property: summing bucket keystroke counts equals the log length.

---

## TI-041 — Metrics: per-key and per-bigram statistics

**Type** feat · **Size** M · **Priority** P1 · **Depends on** TI-035 · **Docs** [GAMEPLAY §4.4](../GAMEPLAY.md#44-latency-and-error-structure)

Attempts, errors, and total latency per grapheme and per bigram. Per-bigram latency is the
metric that most directly answers "what should I practise", and it feeds the drill generator in
Phase 8.

**Unit tests** (`KeyStatsTest.cpp`)
- Attempts and errors tally correctly for a hand-built log.
- Latency is the interval from the previous keystroke, excluding the first.
- Bigrams are formed from **consecutive target positions**, not consecutive keystrokes — a
  backspace does not fabricate a bigram.
- A pause longer than the outlier threshold is excluded from latency averages (documented
  threshold).
- Multi-byte graphemes key correctly (`č` is one key, not two).
- Empty log → empty stats, no crash.

**Acceptance**
- [ ] Bigram formation across a backspace is explicitly tested.
- [ ] Outlier handling is documented and tested.

---

## TI-042 — Metrics: error map

**Type** feat · **Size** S · **Priority** P1 · **Depends on** TI-041

Counts of `expected → typed` substitutions. Feeds the heatmap (Phase 5) and drill selection
(Phase 8).

**Unit tests** (`ErrorMapTest.cpp`)
- A substitution is counted once per first attempt, not once per retype.
- A corrected error still appears in the map (you made it, even though you fixed it).
- Omissions and insertions are classified distinctly from substitutions.
- Multi-byte pairs key correctly.
- Empty log → empty map.

---

## TI-043 — `IMode` interface and `ModeProgress`

**Type** feat · **Size** S · **Priority** P0 · **Depends on** TI-033 · **Docs** [ADR-008](../ARCHITECTURE.md#adr-008--game-modes-are-strategies-behind-one-interface)

Lifecycle hooks (`on_start`, `on_keystroke`, `on_tick`, `is_finished`, `progress`, `id`) plus
the `ModeProgress` variant the HUD renders. The session engine must contain **no `switch` on
mode**.

**Unit tests** (`ModeInterfaceTest.cpp`)
- A `SpyMode` records the hook call order for a scripted session.
- `on_tick` is called even with no keystrokes (required by timed and race modes).
- `is_finished` is stable once true.
- A mode factory returns the right type for every registered id, and an error for an unknown
  id.

---

## TI-044 — `TimedMode`

**Type** feat · **Size** S · **Priority** P0 · **Depends on** TI-043 · **Docs** [GAMEPLAY §2.1](../GAMEPLAY.md#21-timed)

Replaces `Timer`. **The clock starts on the first keystroke**, not on screen entry — the
current implementation starts it when a session flag flips, charging the user for reaction time
and menu latency.

**Unit tests** (`TimedModeTest.cpp`) — with `FakeClock`, **zero sleeping**
- Not finished before the first keystroke, no matter how much time passes.
- Finished exactly at the configured duration after the first keystroke.
- `progress()` reports remaining time correctly at 0%, 50%, 100%.
- Remaining time never goes negative.
- **Ported from `test_timer.cpp`**, all seven cases, with `FakeClock` replacing every
  `sleep_for`: elapsed starts at zero, elapsed after delay, elapsed after max time, remaining
  time immediately, remaining after 1 s, remaining after max, and no counting before start.
- Custom durations at the validated bounds (1 s and 3600 s).

**Acceptance**
- [ ] All seven legacy timer behaviours reproduce **with no sleep and no flakiness** — the
      suite for this file runs in under 10 ms.
- [ ] Time-to-first-keystroke is recorded separately and excluded from the duration.

---

## TI-045 — `WordCountMode`

**Type** feat · **Size** S · **Priority** P1 · **Depends on** TI-043 · **Docs** [GAMEPLAY §2.2](../GAMEPLAY.md#22-words)

**Unit tests** (`WordCountModeTest.cpp`)
- Finishes after exactly N committed words.
- A word counts as committed on the following space or newline, not on its last letter.
- The final word of the text counts without a trailing space.
- Skipped words still count toward N.
- N = 1 and N = 1000 both behave.

---

## TI-046 — `QuoteMode`

**Type** feat · **Size** S · **Priority** P1 · **Depends on** TI-043 · **Docs** [GAMEPLAY §2.3](../GAMEPLAY.md#23-quote)

**Unit tests** (`QuoteModeTest.cpp`)
- Finishes at the last grapheme of the text.
- **Ported**: `InputLineTest.FinishGameOnFullInput`.
- Does not finish early when the user types past the end.
- Empty text → immediately finished (no crash, no OOB).
- Completion percentage is exact at 0%, 50%, 100%.

---

## TI-047 — `ZenMode`

**Type** feat · **Size** XS · **Priority** P2 · **Depends on** TI-043 · **Docs** [GAMEPLAY §2.4](../GAMEPLAY.md#24-zen)

**Unit tests** (`ZenModeTest.cpp`)
- Never reports finished on its own.
- Metrics are still recorded.
- `progress()` reports elapsed rather than remaining.

---

## TI-048 — `ITextProvider` and `WholeTextProvider`

**Type** feat · **Size** S · **Priority** P0 · **Depends on** TI-030 · **Docs** [GAMEPLAY §5.4](../GAMEPLAY.md#54-providers)

The successor to `ITextSource` — the one seam from the current design worth keeping, now
extended to support streaming for endless modes.

**Scope**
- In: the interface (`next_chunk`, `has_more`, `seed`); `WholeTextProvider`; seeded PRNG
  plumbing for reproducibility.
- Out: the other three providers (TI-049, Phase 6).

**Unit tests** (`WholeTextProviderTest.cpp`)
- Yields the whole text once, then reports exhaustion.
- Empty text yields nothing and does not crash.
- The recorded seed reproduces an identical stream on a second run.

---

## TI-049 — `ChunkedProvider`

**Type** feat · **Size** M · **Priority** P2 · **Depends on** TI-048 · **Docs** [GAMEPLAY §2.3](../GAMEPLAY.md#23-quote)

Chunked delivery with a resumable offset, so a long document is typed across many sessions.

**Unit tests** (`ChunkedProviderTest.cpp`)
- Chunks respect the configured size and **never split a word**.
- Resuming from an offset continues at the right grapheme.
- The final chunk may be short; the sum of chunks equals the text.
- Offset beyond the end is clamped and reported.
- Chunk size larger than the text → one chunk.

---

## TI-050 — `Config` value type and pure validation

**Type** feat · **Size** M · **Priority** P1 · **Depends on** TI-025 · **Docs** [TECHNICAL §6](../TECHNICAL.md#6-configuration-file)

The full settings value type with defaults, plus **pure** validation returning `Result`. Parsing
is `infra`'s job (Phase 2); validity is a domain rule.

**Unit tests** (`ConfigValidationTest.cpp`)
- Defaults are valid.
- Every numeric field is rejected outside its documented range (table-driven over all fields).
- Every enum-like string field rejects an unknown value and names the valid set in the error.
- `line_width = 0` is valid and means "fit the terminal".
- Race parameters cross-validate: `lead_danger < lead_comfort`, `min_accuracy ∈ (0,1)`,
  `ramp_up > 0`.
- Validation is pure: the same input always yields the same result, and nothing is mutated.

---

## TI-051 — Port the legacy regression suite

**Type** test · **Size** M · **Priority** P0 · **Depends on** TI-031, TI-033, TI-037, TI-044 · **Docs** [TESTING §10](../TESTING.md#10-regression-tests-carried-over-from-the-current-code)

Walk the mapping table in TESTING §10 and confirm every legacy behaviour worth keeping has a
new home. The existing tests are the specification for the ~40% of current logic that survives.

**Scope**
- In: an audit checklist confirming each of the 41 legacy tests is either ported, deliberately
  superseded, or deliberately dropped — with a reason recorded for every entry.
- Out: keeping the legacy test files alive (deleted at TI-097).

**Acceptance**
- [ ] Every legacy test is accounted for in the checklist.
- [ ] The two deliberate divergences — WPM (TI-036) and accuracy (TI-037) — are recorded with
      the reason, so a later reader cannot mistake them for weakened tests.
- [ ] No legacy behaviour is silently lost.

---

## TI-052 — Coverage gate for `core`

**Type** test · **Size** S · **Priority** P1 · **Depends on** all of Phase 1 · **Docs** [TESTING §9](../TESTING.md#9-coverage)

**Scope**
- In: gcovr in the coverage preset; CI job publishing a summary; thresholds enforced —
  `core/metrics` and `core/text` ≥ 95%, `core` overall ≥ 90%.
- Out: coverage gates for other layers (their phases).

**Acceptance**
- [ ] Thresholds met and enforced; dropping below fails CI.
- [ ] Any uncovered line in `core` is either covered or has a comment explaining why it cannot
      be — there are no dependencies to blame here.

---

## Phase exit criteria

- [ ] `typeit_core` links the standard library and nothing else; the negative build test proves
      it.
- [ ] Coverage thresholds met.
- [ ] `core` test suite runs in **under 2 seconds** with zero sleeps.
- [ ] A perfectly typed run reports 100% accuracy regardless of backspaces (C4 closed).
- [ ] WPM matches GAMEPLAY §4.1 on three hand-verified examples (C5 closed).
- [ ] Empty text and backspace-at-zero are asserted behaviours, not accidents (C7 closed).
- [ ] Every legacy behaviour is accounted for in the TI-051 checklist.
- [ ] The legacy application still builds and passes its own tests, untouched.
