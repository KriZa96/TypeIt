# TypeIt — Test Strategy

*How correctness is established and kept. Conventions live in
[STYLE.md §10](STYLE.md#10-testing-conventions); this document covers what gets tested, how,
and to what standard.*

---

## 1. Where the current suite stands

41 tests across 9 files, and the good news first: the engine tests are real tests. `
InputLineEngine`'s coverage of line transitions, backspacing across a line boundary, the empty
case, and the last-line case shows someone thinking about edge cases rather than chasing a
coverage number.

The problems are structural, and all three disappear under the new architecture:

1. **The suite sleeps.** `TimerTest` and `ScreenTest` between them `sleep_for` about nine
   seconds. `TimerTest.RemainingTimeStringAfter1Sec` sleeps one second and asserts the exact
   string `"9s"` — on a loaded CI runner that is a coin flip. The cause is that `Timer` reads
   `steady_clock` directly, so the only way to test it is to actually wait.
2. **The suite is order-dependent.** Tests mutate `GameState` and `FocusPosition` statics.
   `SetUp()` resets some of them, `test_timer.cpp` does not, and
   `TimerTest.DoesNotCalculateWhenStartGameFalse` depends on what ran before it.
3. **The view and control layers have no tests at all** — `Menu`, `Main`,
   `SpeedTypingSession`, `TextInputArea`, `PerformanceArea`. Not through negligence: those
   classes advance game state inside render callbacks against global flags, so there is no
   seam to test through.

Injecting `IClock` fixes (1). Deleting the globals fixes (2). Separating model from render
fixes (3).

---

## 2. Levels

| Level | Scope | Dependencies | Count | Runtime |
|---|---|---|---|---|
| **Unit** | One class or function | None — pure | ~400 | < 2 s total |
| **Contract** | A port's implementations | In-memory SQLite, temp dirs | ~60 | < 3 s |
| **Service** | App layer use cases | Fake ports + `FakeClock` | ~80 | < 1 s |
| **Render** | TUI components | FTXUI to a string buffer | ~50 | < 2 s |
| **End-to-end** | Whole app, headless | `--simulate` | ~20 | < 5 s |

Target: **the full suite under 15 seconds**, so it can run on every save. That is the real
justification for removing the sleeps — a suite you will not run is not a safety net.

---

## 3. Unit tests — `typeit::core`

This is where the bulk of the value sits, because after ADR-001 the entire domain is pure and
needs no terminal, clock, or database.

### 3.1 Text and segmentation

Table-driven against a fixture list of nasty inputs:

| Input | Expected |
|---|---|
| `"hello"` | 5 graphemes, width 5 |
| `"čšž"` | 3 graphemes, width 3, 6 bytes |
| `"é"` | 1 grapheme (`é`), width 1 |
| `"👨‍👩‍👧"` (ZWJ family) | 1 grapheme, width 2 |
| `"🇭🇷"` (regional indicators) | 1 grapheme, width 2 |
| `"日本語"` | 3 graphemes, width 6 |
| `"a\r\nb"` | 3 graphemes, CRLF as one |
| invalid UTF-8 | `Error{InvalidUtf8}` with a byte offset |

This is the direct answer to the README's note that "FTXUI does not support ćčšđž" — the
behaviour becomes a tested property of our own model rather than an accepted limitation.

### 3.2 Wrapping

`wrap()` is pure, so it is exhaustively testable: exact-fit lines, a word longer than the
line, trailing spaces at a break, wide characters straddling the boundary, width 1, width 0
(rejected), empty text. The behavioural cases from the existing `test_text.cpp` — newline
handling, wrapping at the first space past the threshold — are ported directly, because they
are the specification of behaviour worth keeping.

### 3.3 The typing model

Every transition in the state machine from
[GAMEPLAY §6](GAMEPLAY.md#6-typing-rules), plus:

- backspace at position 0 is a no-op (never underflows)
- backspace over a corrected error returns the position to `Pending`
- typing past the end of the text does not advance the cursor
- `stop_on_error = letter` blocks input until the error is corrected
- a skipped word marks the intervening positions `Missed`
- an empty target text produces a model that is immediately finished

That last one deserves a note: with an empty text the current implementation is saved from an
out-of-bounds write only by an unrelated early return in `should_finish_game()`
([review C7](CODEBASE_REVIEW.md#4-correctness-defects)). Here it is an explicit, asserted
behaviour.

### 3.4 Metrics — property tests

These are the highest-value tests in the project, because they encode the definitions in
[GAMEPLAY §4](GAMEPLAY.md#4-metrics--exact-definitions) as executable properties:

| Property | Why it matters |
|---|---|
| A perfectly typed run has accuracy 100% **regardless of how many backspaces it contains** | This is exactly [defect C4](CODEBASE_REVIEW.md#4-correctness-defects). Type wrong, correct it, and accuracy must reflect one first-attempt error — not two samples, and not a permanent penalty |
| Typing and deleting the same passage *n* times does not change the denominator | The unbounded-inflation half of the same defect |
| `net_wpm ≤ gross_wpm ≤ raw_wpm`, always | Ordering invariant |
| Doubling every timestamp halves every WPM | Rate correctness |
| Metrics are invariant under log replay | Purity — the guarantee ADR-002 is built on |
| Consistency is 100 for a perfectly even run, and lower for a bursty one with identical totals | The metric actually measures what it claims |
| An empty log yields zeros, never NaN or a division by zero | The current `WordCalculatorEngine` special-cases `elapsed <= 1`; here it is a tested boundary |

Fixtures are synthetic `KeystrokeLog`s built by a small DSL:

```cpp
auto log = LogBuilder{}.type("h", 100ms).type("q", 200ms)
                       .backspace(300ms).type("e", 400ms).build();
```

No terminal, no clock, no timing dependency, no flakiness.

### 3.5 Race mode

`DifficultyController` is pure arithmetic, so its behaviour is asserted directly against
scripted `(lead, accuracy, Δt)` sequences:

- speed never rises while accuracy is below `A_min`, no matter how large the lead
- speed rises monotonically with lead above `lead_comfort`
- speed is unchanged inside the dead band (the hysteresis that stops the pacer stuttering)
- speed falls at `k_down` in the danger zone
- speed is clamped to `[V_min, V_max]`
- a stumble followed by recovery returns to climbing (no death spiral)
- a full scripted run produces a monotone-ish curve matching a golden reference

`Pacer` tests cover the grace period, position advance for a given speed, push-back on losing
a life, and the catch condition including the grace window.

---

## 4. Contract tests — ports

Each port gets one shared test suite run against every implementation, so a fake and a real
adapter cannot silently disagree.

```cpp
template <typename RepoFactory>
class HistoryRepositoryContract : public ::testing::Test { … };

using Implementations = ::testing::Types<SqliteRepoFactory, FakeRepoFactory>;
TYPED_TEST_SUITE(HistoryRepositoryContract, Implementations);
```

`SqliteHistoryRepository` is tested against a **real in-memory database** (`:memory:`), not a
mock — the SQL is the part most likely to be wrong, so mocking it would test nothing. Coverage
includes migrations from every prior schema version, foreign-key cascades, the
`ON CONFLICT DO UPDATE` merge paths for key/bigram stats, personal-best replacement, and
rejection of a future `user_version`.

Filesystem-touching tests use a temp directory fixture that cleans up in its destructor, with
`TYPEIT_DATA_DIR` and `TYPEIT_CONFIG_DIR` pointed at it — so tests never touch the developer's
real history.

---

## 5. Service tests — `typeit::app`

Fake ports plus `FakeClock`. What is asserted here is orchestration, not arithmetic:

- `SessionService` persists exactly one session row, N sample rows, and merged key stats, in a
  single transaction
- a failed save leaves the database unchanged (transaction rollback)
- an abandoned run is stored with `completed = 0` and does **not** update personal bests
- `TextLibraryService` deduplicates by content hash, and re-importing the same file returns the
  existing id
- normalisation is applied in the documented order and is idempotent
- `ProfileService` computes race start speed as `max(V_floor, α × best_sustained)` and falls
  back correctly with no history
- `DrillService` selects targets by the documented priority function and degrades gracefully
  when history is sparse

---

## 6. Render tests — `typeit::tui`

FTXUI can render a component into a `Screen` and expose the result as a string, which makes
snapshot testing straightforward and is currently unused:

```cpp
auto screen = ftxui::Screen::Create(ftxui::Dimension::Fixed(80), ftxui::Dimension::Fixed(24));
ftxui::Render(screen, component->Render());
EXPECT_EQ(screen.ToString(), golden);
```

Covered:

- `TypingArea` renders correct / incorrect / pending / corrected states with the right
  attributes, at 80×24, 120×40, and 60×20
- an incorrect space renders as `_`
- the caret lands in the right cell after wide characters and combining marks
- the race pacer marker is positioned correctly relative to the caret
- ASCII glyph fallback produces no non-ASCII bytes anywhere in the output
- every theme renders identically in structure at all four colour depths
- the terminal-too-small screen appears below 80×24
- **`Render()` mutates nothing**: render the same component twice and assert the model is
  byte-identical. This is a direct regression guard against
  [the render-side-effect pattern](CODEBASE_REVIEW.md#32-the-model-updates-itself-while-rendering)
  in the current code

Interaction is driven through `OnEvent`, asserting on resulting model state — no sleeping, no
real terminal.

Golden files are stored as text and reviewed like code; a diff in a snapshot is a diff a human
reads.

---

## 7. End-to-end tests

`typeit --simulate <script>` runs the real application stack — real services, real SQLite (in a
temp dir), real modes — driven by a scripted keystroke log with a `FakeClock`, and prints
metrics as JSON. No terminal is involved.

```
# script.tks
100  type h
250  type e
400  backspace
520  type e
```

Asserted end to end:

- a full 30-second timed run produces the expected metrics and exactly one database row
- a race run against a scripted "player" ends when the pacer catches them, at the expected
  time
- `provider_seed` reproduces an identical text stream on a rerun — the property that turns a
  bug report into a deterministic repro
- import → type → history → export round-trips without data loss
- a corrupt database is reported rather than silently recreated
- a malformed config file is reported and never overwritten

---

## 8. Non-functional checks

| Check | How |
|---|---|
| **Sanitizers** | Every test runs under ASan + UBSan in one CI job |
| **Performance** | Micro-benchmarks assert the budgets in [ARCHITECTURE §6.5](ARCHITECTURE.md#65-performance-budget): keystroke-to-frame < 5 ms, rolling WPM O(window), history aggregation over 10k sessions < 200 ms |
| **Fuzzing** | libFuzzer targets on every input the program did not produce: `TextBuffer::from_utf8`, the TOML config parser, the keystroke-script parser, and — once [text ingestion](TEXT_SOURCES.md) lands — the Markdown, subtitle, EPUB, and HTML extractors. The EPUB and HTML targets are the highest-value ones in the project, because they parse container and markup formats from sources the user does not control. Full target list in [TI-141](issues/PHASE-9-release.md#ti-141--fuzz-targets) |
| **PTY smoke test** | The built binary launched in a pseudo-terminal on Linux and Windows: first frame, quit key, clean exit, console mode restored. Snapshot tests check bytes; this is the only thing that exercises real terminal initialisation ([CI-017](issues/PHASE-0A-cicd.md#ci-017--pty-smoke-test)) |
| **Memory** | A long endless run under a soak test shows bounded growth; the keystroke log is the only structure that grows, and its growth is linear and measured |
| **Static analysis** | clang-tidy clean; `bugprone-*`, `cppcoreguidelines-*`, `performance-*`, `readability-*` enabled |

---

## 9. Coverage

Line coverage is a diagnostic, not a target. The gates that matter:

| Area | Expectation |
|---|---|
| `core/metrics`, `core/text`, `core/modes` | ≥ 95% — pure, cheap to cover, and where being wrong is silent |
| `core` overall | ≥ 90% |
| `app` | ≥ 85% |
| `infra` | ≥ 75% — some error paths need real I/O failures |
| `tui` | ≥ 60% — rendering is snapshot-covered, wiring is not |

Uncovered code in `core` is treated as a review finding, because there is no excuse for it:
there are no dependencies to stand up.

---

## 10. Regression tests carried over from the current code

The existing suite is the specification for behaviour worth preserving. Each of these is
ported to the new architecture in Phase 1 and must pass:

| Current test | Ported as |
|---|---|
| `TextTest.*` (newline handling, 55-column wrapping, multiple newlines) | `WrapperTest`, `TextBufferTest` |
| `InputLineTest.LineTransitionOnSpace`, `BackspaceAtLineStart`, `ShouldGoToPreviousLine` | `TypingModelTest` cursor and backspace cases |
| `InputLineTest.FinishGameOnFullInput` | `QuoteModeTest.FinishesAtEndOfText` |
| `InputLineTest.DoNothingWhenNoElements`, `ShouldRemoveFirstElement` | `TypingModelTest` boundary cases — now asserted rather than accidental |
| `InputWordCountTest.*` (8 whitespace cases) | `TextBufferTest.WordCount` |
| `WordCalculatorTest.*` | `MetricsTest` — **with the formula corrected** per [GAMEPLAY §4](GAMEPLAY.md#4-metrics--exact-definitions); these tests change expected values deliberately, and the change is documented in the commit |
| `InputAccuracyTest.*` | `MetricsTest.Accuracy*` — **with the semantics corrected**; the backspace behaviour these tests currently lock in is the defect |
| `FileTextSourceTest.*` | `TextLibraryServiceTest` + a **new** test for the empty-file case that currently triggers UB |
| `TimerTest.*` | `TimedModeTest` with `FakeClock` — same assertions, zero sleeping |
| `ScreenTest.*` | `FrameTickerTest` — start/stop lifecycle without sleeping |

Two entries deliberately change expected values: the WPM tests and the accuracy tests. That is
the point — those tests currently assert the defective behaviour described in
[review C4 and C5](CODEBASE_REVIEW.md#4-correctness-defects). Changing them is the fix, and
saying so explicitly here is what keeps it from looking like a test was weakened to make a
build pass.
