# Legacy test audit (TI-051)

Every test in the 1.0 suite, and what became of it. The old tests are the specification for
the roughly 40% of the current logic that survives, so "we rewrote it and the new tests pass"
is not an argument — this table is.

**Written 2026-08-03, against `tests/test_*.cpp` at the tip of `v2`.**

Four outcomes are possible, and every row has exactly one:

| Outcome | Meaning |
|---|---|
| **Ported** | The same behaviour, asserted again in the new suite. |
| **Superseded** | The behaviour changed on purpose. The reason is in the row and in the commit. |
| **Deferred** | The behaviour belongs to a layer that does not exist yet. The issue that will carry it is named. |
| **Dropped** | The test asserted an implementation detail that no longer exists. What it protected, if anything, is named. |

[TESTING §10](TESTING.md#10-regression-tests-carried-over-from-the-current-code) counts 41
legacy tests. The actual count is **64**, across ten files; the table there groups by file and
undercounts. Every one of the 64 appears below.

---

## `test_text.cpp` — 10 tests · **Ported**

Wrapping and line counting, now split between `TextBuffer` (TI-030) and the pure `wrap()`
(TI-031).

| Legacy test | Now |
|---|---|
| `TextTest.TestTextEmpty` | `WrapperTest.EmptyTextHasNoLines`, `TextBufferTest.EmptyTextIsAValidBuffer` |
| `TextTest.TestTextNotEmpty` | `WrapperTest.ReproducesTheLegacyLineCounts` |
| `TextTest.TestWithnewlineChar` | `WrapperTest.ReproducesTheLegacyLineCounts` |
| `TextTest.TestWithOnlyNewlineChar` | `WrapperTest.ReproducesTheLegacyLineCounts` |
| `TextTest.TestWithNewlineCharAndOneChar` | `WrapperTest.ReproducesTheLegacyLineCounts` |
| `TextTest.TestWithSpacesOneLine` | `WrapperTest.ReproducesTheLegacyLineCounts` |
| `TextTest.TestWithSpacesTwoLine` | `WrapperTest.ReproducesTheLegacyLineCounts` |
| `TextTest.TestWithSpacesThreeLines` | `WrapperTest.ReproducesTheLegacyLineCounts` |
| `TextTest.TestWithSpacesAndNewLineThreeLines` | **Superseded** — `WrapperTest.DivergesFromTheLegacyCountWhereTheLegacyOverflowed`. 1.0 emitted a 57-column line into a 55-column view; four lines is the honest answer and the test says why. |
| `TextTest.WrapConditionUnchangedAfterParenthesisation` | `WrapperTest.ReproducesTheLegacyLineCounts` — the threshold cases (53 and 54 a's) are asserted directly. |

## `test_word_count.cpp` — 10 tests · **Ported**

All ten, with identical expectations, in `TextBufferWordCountTest.MatchesTheLegacyCounts`. A
WPM figure has to mean the same thing before and after the rebuild, so the old suite's answers
are kept exactly.

| Legacy test | Now |
|---|---|
| `InputWordCountTest.SetWordsNone` | `MatchesTheLegacyCounts` (`""` → 0) |
| `InputWordCountTest.SetWordsOne` | `MatchesTheLegacyCounts` (`"one"` → 1) |
| `InputWordCountTest.SetWordsOneSpaceAround` | `MatchesTheLegacyCounts` (`" one "` → 1) |
| `InputWordCountTest.SetWordsOneSpaceAfter` | `MatchesTheLegacyCounts` (`"one "` → 1) |
| `InputWordCountTest.SetWordsOneSpaceBefore` | `MatchesTheLegacyCounts` (`" one"` → 1) |
| `InputWordCountTest.SetWordsTwo` | `MatchesTheLegacyCounts` (`"one two"` → 2) |
| `InputWordCountTest.SetWordsTwoWithTwoSpacesBetween` | `MatchesTheLegacyCounts` (`"one  two"` → 2) |
| `InputWordCountTest.SetWordsTwoWithMultipleSpacesBetweenAndAround` | `MatchesTheLegacyCounts` |
| `InputWordCountTest.SetWordsTwoNewLine` | `MatchesTheLegacyCounts` (`"one\ntwo"` → 2) |
| `InputWordCountTest.SetWordsWordCountCalculation` | `MatchesTheLegacyCounts` (`"one two three"` → 3) |

Extended rather than merely ported: `TabsAndCarriageReturnsSeparateWords`,
`ANonBreakingSpaceDoesNotSeparateWords` and `TextWithoutSpacesIsOneWord` pin down cases 1.0
never asked about.

## `test_input_line_engine.cpp` — 12 tests

Six port, one moves to `QuoteMode`, five are dropped. The legacy engine stored the input as
lines and counted them; the rebuild has one flat cursor and computes lines separately (TI-031),
so a test about `get_current_line_index()` is a test about a class that no longer exists. Every
*behaviour* those five covered is covered by a cursor assertion.

| Legacy test | Outcome | Now |
|---|---|---|
| `InputLineTest.LineTransitionOnSpace` | Ported | `LegacyInputLineTest.LineTransitionOnSpace` |
| `InputLineTest.BackspaceAtLineStart` | Ported | `LegacyInputLineTest.BackspaceAtLineStartReturnsToThePreviousLine` |
| `InputLineTest.ShouldGoToPreviousLine` | Ported | `LegacyInputLineTest.BackspaceAtLineStartReturnsToThePreviousLine` — the same behaviour under two legacy names |
| `InputLineTest.DoNothingWhenNoElements` | Ported | `LegacyInputLineTest.DoNothingWhenNoElements`, and `TypingModelTest.BackspaceAtTheStartIsANoOpAndLogsNothing` — asserted now rather than accidental (defect C7) |
| `InputLineTest.ShouldRemoveFirstElement` | Ported | `LegacyInputLineTest.RemovesTheFirstElement` |
| `InputLineTest.ShouldNotAddOrRemoveElement` | Ported | `LegacyInputLineTest.TypingMoreThanTheTextAddsNothing` |
| `InputLineTest.FinishGameOnFullInput` | Ported | `QuoteModeTest.LegacyFinishGameOnFullInput` |
| `InputLineTest.Initialization` | Dropped | Asserted `get_total_input_lines().size() == lines + 1`, a property of the engine's internal line vector. There is no such vector; `TypingModelTest` asserts the states array is the length of the text, which is the invariant that mattered. |
| `InputLineTest.ShouldGoToNextLine` | Dropped | Same behaviour as `LineTransitionOnSpace`, asserted through the line index. Covered by the ported cursor case. |
| `InputLineTest.ShouldNotGoToNextLine` | Dropped | "Typing a word without its trailing space does not advance the line." Covered by `LegacyInputLineTest.LineTransitionOnSpace`, which asserts the cursor is where the text is rather than which line it is on. |
| `InputLineTest.ShouldNotGoToNextLineLastLine` | Dropped | Asserted the line index saturates at the last line. The cursor equivalent — typing past the end changes nothing — is `TypingModelTest.TypingPastTheEndChangesNothing`. |
| `InputLineTest.ShouldNotGoToPreviousLine` | Dropped | Asserted that backspacing mid-line does not change the line index. Covered by `TypingModelTest.BackspaceOverAnErrorReturnsThePositionToPending`. |

## `test_timer.cpp` — 7 tests · **Ported**

All seven, in `TimedModeTest`, with `FakeClock` discipline replacing every `sleep_for`. The
legacy file spends eight real seconds racing the scheduler to assert the string `"9s"`; the
ported tests report 0.01 s each.

| Legacy test | Now |
|---|---|
| `TimerTest.ElapsedTimeStartsAtZero` | `TimedModeTest.LegacyElapsedTimeStartsAtZero` |
| `TimerTest.ElapsedTimeAfterDelay` | `TimedModeTest.LegacyElapsedTimeAfterDelay` |
| `TimerTest.ElapsedTimeAfterMaxTime` | `TimedModeTest.LegacyElapsedTimeAfterMaxTime` |
| `TimerTest.RemainingTimeImmediate` | `TimedModeTest.LegacyRemainingTimeImmediate` |
| `TimerTest.RemainingTimeStringAfter1Sec` | `TimedModeTest.LegacyRemainingTimeAfterOneSecond` — the value, not the string; formatting is the TUI's job |
| `TimerTest.RemainingTimeStringAfterMaxTime` | `TimedModeTest.LegacyRemainingTimeAfterMaxTime` |
| `TimerTest.DoesNotCalculateWhenStartGameFalse` | `TimedModeTest.LegacyDoesNotCountBeforeTheRunStarts` — and strengthened: the clock now starts on the first keystroke rather than on a global flag |

## `test_word_calculator.cpp` — 4 tests · **Superseded**

The formula is corrected per [GAMEPLAY §4.1](GAMEPLAY.md#41-speed) and the expected values
change deliberately (defect C5). Recorded in the TI-036 commit and in
`MetricsSpeedTest.DivergesFromTheLegacyWordCalculator`.

| Legacy test | What it asserted | Now |
|---|---|---|
| `WordCalculatorTest.TestWordsPerMinute60Words` | 60 whitespace-delimited "words" in 60 s → 60 WPM | 60 graphemes in 60 s → **12** WPM. A word is five graphemes, as in every other typing test. |
| `WordCalculatorTest.TestWordsPerMinuteAbove60Words` | 120 → 120 WPM | Same correction; `MetricsSpeedTest.ThreeHundredGraphemesInSixtySecondsIsSixtyWordsPerMinute` is the equivalent worked example. |
| `WordCalculatorTest.TestWordsPerMinuteBelow60Words` | 30 → 30 WPM | Same correction. |
| `WordCalculatorTest.TestWordsPerMinuteElapsedTime0` | 0 elapsed → 0 WPM | **Kept exactly**: `MetricsSpeedTest.AnEmptyLogIsAllZerosAndNoNaN` and the closing assertion of `DivergesFromTheLegacyWordCalculator`. |

## `test_input_accuracy.cpp` — 3 tests · **Superseded**

The semantics are corrected (defect C4). Two of the three were right and carry over; the third
locks in the defect.

| Legacy test | Outcome | Now |
|---|---|---|
| `InputAccuracyTest.PercentageOfCorrectInput` | Ported | `MetricsAccuracyTest.KeepsTheLegacyCasesThatWereRight` — 100 correct keystrokes is still 100% |
| `InputAccuracyTest.PercentageOfCorrectInputOnStart` | Ported | Same test — nothing typed is still 0%, and still not a NaN |
| `InputAccuracyTest.PercentageOfCorrectInput50Percent` | Superseded | Still 50% when neither keystroke is corrected (asserted). The legacy engine says 50% **even after the mistake is fixed**, which is the defect; `RegressionForDefectC4WrongThenBackspaceThenRightIsOneAttempt` asserts the corrected behaviour. |

## `test_file_text_source.cpp` — 8 tests · **Deferred to Phase 2**

Reading a file is `infra`'s job now (ADR-013). These port to `FileFetcher` and
`TextLibraryService` tests in Phase 2, plus the **new** empty-file case that currently triggers
undefined behaviour.

| Legacy test | Deferred to |
|---|---|
| `FileTextSourceTest.ReadsFileCorrectlyWithoutNewLine` | Phase 2 — `FileFetcherTest` |
| `FileTextSourceTest.ReadsFileCorrectlyWithNewLine` | Phase 2 — `FileFetcherTest` |
| `FileTextSourceTest.ReadsEmptyFileReturnsEmptyString` | Phase 2 — `FileFetcherTest`, plus the new UB case |
| `FileTextSourceTest.ReadsWhitespaceOnlyFile` | Phase 2 — `FileFetcherTest` |
| `FileTextSourceTest.ReadsSingleCharacterFile` | Phase 2 — `FileFetcherTest` |
| `FileTextSourceTest.IsFileValidTrue` | Phase 2 — `FileFetcherTest` |
| `FileTextSourceTest.IsFileValidFalse` | Phase 2 — `FileFetcherTest` |
| `FileTextSourceTest.IsFileValidEmptyFile` | Phase 2 — `FileFetcherTest` |

The domain half of these is already covered: `TextBufferTest.EmptyTextIsAValidBuffer` and
`ChunkedProviderTest.AnEmptyTextHasNothingToDeliver` assert that an empty text reaches the
model without incident, which is what the UB case was really about.

## `test_screen.cpp` — 5 tests · **Deferred to Phase 4**

Frame-ticker lifecycle, which belongs to the TUI (ADR-012). They port to `FrameTickerTest`,
without sleeping.

| Legacy test | Deferred to |
|---|---|
| `ScreenTest.TestIsRunning` | Phase 4 — `FrameTickerTest` |
| `ScreenTest.TestIsRunningAfterSomeTime` | Phase 4 — `FrameTickerTest` |
| `ScreenTest.TestIsStopped` | Phase 4 — `FrameTickerTest` |
| `ScreenTest.TestIsStoppedAfterSomeTime` | Phase 4 — `FrameTickerTest` |
| `ScreenTest.TestIsRunningAndStoppedAfterSomeTime` | Phase 4 — `FrameTickerTest` |

## `test_input.cpp` — 2 tests · **Deferred to Phase 4**

FTXUI event handling against the global `GameState` flags. The flags are replaced by
`ScreenStack` (ARCHITECTURE §5.3), so these port as navigation tests rather than as flag tests.

| Legacy test | Deferred to |
|---|---|
| `InputTest.EventHandlingTerminateMenu` | Phase 4 — `ScreenStackTest` |
| `InputTest.EventHandlingTerminateRefresh` | Phase 4 — `ScreenStackTest` |

## `test_version.cpp` — 3 tests · **Kept**

Not legacy behaviour at all: added in Phase 0A alongside the version header, still building,
still passing, and still guarding the same thing.

---

## Summary

| Outcome | Tests |
|---|---|
| Ported | 36 |
| Superseded, deliberately | 5 |
| Dropped | 5 |
| Deferred to a later phase | 15 |
| Kept as they are | 3 |
| **Total** | **64** |

Nothing is unaccounted for. The five superseded tests are the two documented divergences —
WPM (TI-036) and accuracy (TI-037) — plus the wrapping overflow (TI-031); each is a defect
being fixed rather than a test being weakened, and each says so in its own test file as well as
here. The five dropped tests all asserted the line-index bookkeeping of a class the rebuild
does not have, and each names the cursor assertion that covers the same behaviour.
