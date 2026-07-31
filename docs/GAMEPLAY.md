# TypeIt — Gameplay Design

*What the game actually is, how each mode behaves, and exactly how every number on screen is
computed. This document is the specification the tests in
[TESTING.md](TESTING.md) assert against.*

---

## 1. Design principles

1. **Measure honestly.** Every metric uses a published, comparable definition. A user's WPM
   in TypeIt should mean the same thing as their WPM anywhere else.
   [The current implementation does not satisfy this](CODEBASE_REVIEW.md#4-correctness-defects)
   and §4 defines the replacement.
2. **A mistake corrected is not a mistake repeated.** Correcting an error must improve the
   final text, and the metrics must distinguish "made an error" from "left an error in".
3. **Difficulty adapts to the player, not to a table.** Race mode's starting speed comes from
   the player's own history.
4. **The ramp is a teacher, not a filter.** Race mode is designed to hold the player slightly
   above their comfortable speed for as long as possible, because that is where the learning
   is — not to end the run as quickly as possible.
5. **Every run is data.** Nothing is discarded. Abandoned runs are recorded and flagged, not
   deleted, because "how often do I quit at 40 seconds" is itself a useful signal.

---

## 2. Modes

| Mode | Ends when | Text supply | Primary score |
|---|---|---|---|
| **Timed** | Timer reaches zero | Provider streams as needed | Net WPM |
| **Words** | N words completed | Provider streams as needed | Net WPM |
| **Quote** | End of the selected text | One fixed text | Net WPM + completion time |
| **Zen** | User stops | Provider streams as needed | Nothing (practice) |
| **Endless** | User stops | Infinite provider | Distance + rolling WPM |
| **Race** | Pacer catches the player | Infinite provider | Peak sustained WPM |
| **Drill** | N repetitions | Generated from weak keys | Error rate on target keys |

### 2.1 Timed

Durations 15 / 30 / 60 / 120 s, plus a custom value (1–3600 s, validated). The clock starts
on the **first keystroke**, not on screen entry — the current implementation starts it when
the session flag flips, which charges the user for their reaction time and menu latency.
Time-to-first-keystroke is recorded separately as its own metric.

### 2.2 Words

10 / 25 / 50 / 100 words, plus custom. A word is counted as complete when the player commits
the space (or newline) that follows it.

### 2.3 Quote

Type a specific text end to end. For texts longer than the configured chunk size, the run
covers one chunk and a **bookmark** advances, so a whole book can be typed across many
sessions. Completion percentage is shown in the text library.

### 2.4 Zen

No timer, no target, no fail condition. Metrics are still recorded. This exists because
warm-up without pressure is a real part of learning to type, and because it is the honest
answer for a user who just wants to type their own text.

### 2.5 Endless

The text never runs out. `WordPoolProvider` or `ShuffledSentenceProvider` generates a
continuous stream (see §5). The HUD shows a **rolling** WPM over the last 15 seconds rather
than a cumulative average, because in a run with no end a cumulative average stops responding
to what you are doing now. The player stops with the quit key; distance typed and peak rolling
WPM are recorded.

Endless is mechanically Race mode with the pacer disabled, and is implemented that way.

### 2.6 Drill

Generated practice. `DrillService` reads the player's `key_stat`, `bigram_stat`, and
`error_pair` history, picks the *n* worst-performing targets (weighted by error rate ×
frequency × recency), and synthesises drill text that concentrates them — real words
containing the target bigrams where possible, falling back to generated sequences. This is the
mechanism that turns recorded history into actual improvement instead of just a chart.

---

## 3. Race mode — the progressive ramp

This is the headline mode. The design goal is stated precisely: **keep the player in the band
just above their sustainable speed for as long as possible, and let that band drift upward
across sessions.**

### 3.1 Mechanics

A **pacer** — a ghost cursor — advances through the text at a target speed `V(t)`, measured in
WPM. The player types ahead of it. The gap between them is the **lead**:

```
lead(t) = player_index(t) − pacer_index(t)      [in graphemes]
```

- Sustain a healthy lead with good accuracy → `V` increases.
- Fall into the danger zone or let accuracy drop → `V` decreases.
- `lead ≤ 0` for longer than the grace period → the pacer catches you and the run ends.

The pacer's position advances continuously:

```
pacer_index(t + Δ) = pacer_index(t) + Δ · V(t) · 5 / 60
```

(the standard five-graphemes-per-word convention, consistent with §4).

### 3.2 The ramp law

```
V(t + Δ) = clamp( V(t) + Δ · r(lead, A) , V_min , V_max )
```

where `A` is rolling accuracy over the last `W_acc` graphemes (default 50), and

```
           ┌  +k_up · f(lead) · g(A)     if lead ≥ lead_comfort and A ≥ A_min
    r =    ┤   0                          if lead_danger < lead < lead_comfort   (dead band)
           └  −k_down                     if lead ≤ lead_danger  or  A < A_min
```

with

```
f(lead) = clamp( (lead − lead_comfort) / lead_scale , 0 , 1 )
g(A)    = clamp( (A − A_min) / (1 − A_min) , 0 , 1 )
```

**Why each piece is there:**

- **`f(lead)`** makes the ramp proportional to how comfortably the player is winning. A
  player barely holding a 26-grapheme lead gains almost nothing; a player 60 graphemes ahead
  climbs fast. This is what keeps the player near their edge instead of grinding through
  speeds they have already mastered.
- **`g(A)`** gates *all* speed gain on accuracy. Typing garbage quickly earns nothing. This is
  the single most important term for the "learn to type faster" goal — without it the mode
  teaches the player to mash keys.
- **The dead band** between `lead_danger` and `lead_comfort` provides hysteresis. Without it
  `V` oscillates every tick around the boundary and the pacer visibly stutters.
- **`k_down > k_up`** (default 1.5 vs 0.6 WPM/s) means the ramp backs off faster than it
  advances. A player who stumbles gets breathing room rather than a death spiral. Recovery is
  possible; that is deliberate.

### 3.3 Failure and grace

- The run ends when `lead ≤ 0` continuously for `grace_ms` (default 300 ms). A single
  fumbled keystroke does not end a run; a sustained inability to keep up does.
- With `lives > 1`, being caught consumes a life instead: the pacer is pushed back to
  `lead_comfort` behind the player, `V` drops by `catch_penalty` (default 10%), and play
  continues. Reaching zero lives ends the run.
- A **5-second grace period** at the start, during which the pacer does not move, lets the
  player get going.

### 3.4 Starting speed — where progression lives

```
V_0 = max( V_floor , α · best_sustained_wpm_last_30_days )
```

with `α = 0.85` and `V_floor = 20`. A first-ever run starts at 20 WPM.

`best_sustained_wpm` is the highest `V` the player held for at least `sustain_window` seconds
(default 10) in any recorded race — **not** the peak instantaneous value, which any lucky
burst can inflate.

This is the whole progression loop: today's ceiling becomes tomorrow's floor, so the player
never re-grinds speeds they have already proven, and the ramp always starts near the edge of
their ability. Over weeks the trend chart in the history screen is a direct picture of
improvement.

### 3.5 Presets

| Parameter | Gentle | Standard | Brutal |
|---|---|---|---|
| `k_up` (WPM/s) | 0.35 | 0.60 | 1.00 |
| `k_down` (WPM/s) | 2.00 | 1.50 | 1.00 |
| `A_min` | 0.88 | 0.92 | 0.95 |
| `lead_comfort` (graphemes) | 35 | 25 | 15 |
| `lead_danger` (graphemes) | 12 | 8 | 4 |
| `lead_scale` | 40 | 30 | 20 |
| `grace_ms` | 600 | 300 | 100 |
| `lives` | 3 | 1 | 1 |
| `α` (start factor) | 0.75 | 0.85 | 0.95 |

Every value is overridable in `[race]` in the config file; `preset = "custom"` uses the
explicit values.

### 3.6 Post-race analysis — the speed wall

When a race ends, the results screen reports the **speed wall**: the WPM band in which the
player's rolling accuracy first fell below `A_min` and did not recover. Reported alongside it
are the five grapheme pairs that failed most often *inside that band* — which are, by
construction, the keys that are actually limiting the player's speed, as opposed to the ones
they get wrong when typing comfortably.

The screen offers a one-key jump into a Drill built from exactly those pairs. That is the
loop: race → find the wall → drill the wall → race again.

---

## 4. Metrics — exact definitions

All metrics are pure functions of the `KeystrokeLog` and the target text (ADR-002). The log
records, per event: timestamp (ms since run start), kind (`Type` / `Backspace` / `Commit`),
the grapheme typed, and the target index it applied to.

Let:

- `T` = elapsed run time in minutes, from the first keystroke to the last (or to the timer
  expiry, whichever ends the run). **Never clamped** — the current implementation clamps
  elapsed time to the configured duration, which distorts every rate computed from it.
- `C_all` = total graphemes entered (excluding backspaces)
- `C_correct` = graphemes whose **final** state matches the target
- `E_uncorrected` = graphemes whose final state does not match the target
- `E_total` = distinct target positions that were wrong at any point

### 4.1 Speed

| Metric | Definition |
|---|---|
| **Raw WPM** | `(C_all / 5) / T` — everything you typed, mistakes included |
| **Gross WPM** | `(C_correct / 5) / T` — the standard headline number |
| **Net WPM** | `Gross − (E_uncorrected / T)` — the standard penalised figure, floored at 0 |
| **Rolling WPM** | Gross WPM over a sliding window (default 15 s), for the live HUD and endless mode |
| **Peak sustained WPM** | Highest rolling WPM held for ≥ 10 s |

The five-graphemes-per-word convention is used throughout, including the leading space of a
word, which is what makes the number comparable with every other typing test. TypeIt reports
**Net WPM** as the primary figure and shows Gross and Raw alongside it.

The current implementation's "count whitespace-delimited words in whatever the user typed,
including wrong ones, and divide by a clamped time" is replaced entirely.

### 4.2 Accuracy

| Metric | Definition |
|---|---|
| **Accuracy** | `(target positions correct on first attempt) / (target positions attempted)` |
| **Final correctness** | `C_correct / C_all` — how much of the finished text is right |
| **Correction rate** | `(errors corrected) / E_total` |
| **Backspace rate** | `backspaces / C_all` |

The critical property, and the direct fix for
[defect C4](CODEBASE_REVIEW.md#4-correctness-defects): typing `x`, backspacing, and typing the
correct `a` yields **one** attempt at that position, marked as a first-attempt error. It does
not add a second sample. Repeatedly deleting and retyping a passage does not change the
denominator at all. This falls out of deriving accuracy from the log rather than accumulating
a `std::vector<bool>`.

### 4.3 Consistency

```
consistency = 100 × (1 − σ / μ)
```

over the per-second Gross WPM samples, floored at 0. This is the widely used definition
(σ = standard deviation, μ = mean). It answers "do you type at a steady rate or in bursts",
which is a better predictor of real-world typing throughput than peak speed.

### 4.4 Latency and error structure

| Metric | Use |
|---|---|
| **Per-grapheme mean latency** | Which individual keys are slow |
| **Per-bigram mean latency** | Which *transitions* are slow — the actually useful signal |
| **p50 / p95 inter-key interval** | Distinguishes "consistently slow" from "occasionally stalls" |
| **Error pairs** (`expected → typed`) | Feeds the heatmap and the drill generator |
| **Time to first keystroke** | Reaction; excluded from the WPM denominator |

Per-bigram latency is the metric that most directly maps to "what should I practise", which is
why `bigram_stat` is a first-class table in the schema
([TECHNICAL.md §5](TECHNICAL.md)).

---

## 5. Text supply — "type anything you want"

### 5.1 Getting text in

| Route | Command / UI |
|---|---|
| File | `typeit --text ./notes.md`, or Import in the Text Library screen |
| Stdin | `cat article.txt \| typeit -` |
| Paste | Import → Paste in the Text Library screen |
| Bundled | The simple / medium / hard corpora, installed to the platform data dir |
| By id | `typeit --text-id 42` |

### 5.2 Normalisation at import

Applied in order, each individually toggleable:

1. Decode as UTF-8; reject and report invalid sequences with a byte offset.
2. Normalise line endings (CRLF/CR → LF).
3. Unicode NFC normalisation.
4. Optionally flatten typographic characters — curly quotes, en/em dashes, ellipses — to
   ASCII equivalents. **On by default**, because `"` is on the keyboard and `"` is not, and a
   typing test that is unpassable is not a typing test.
5. Collapse runs of whitespace; strip trailing whitespace per line.
6. Optionally strip punctuation and/or lowercase ("simplify" toggle).
7. Expand tabs to spaces (configurable width).
8. Compute SHA-256 of the normalised content for deduplication.

Normalisation is a pure function in `core` and is unit-tested against a table of nasty inputs.
The original is stored alongside the normalised form so settings can be changed later without
re-importing.

### 5.3 Difficulty scoring

Each imported text gets an automatic score from measurable properties: mean word length,
proportion of words outside a common-word list, punctuation density, capitalisation density,
digit density, and non-ASCII density. The score is normalised to 1–10 and is advisory —
displayed in the library, usable as a filter, never used to gate anything.

### 5.4 Providers

| Provider | Behaviour |
|---|---|
| `WholeTextProvider` | The text, once. Quote mode. |
| `ChunkedProvider` | One chunk at a time with a persistent bookmark. Long documents. |
| `ShuffledSentenceProvider` | Sentences from the text in random order, forever. |
| `WordPoolProvider` | Extracts the vocabulary of the text (with frequency weights) and generates an endless stream in that text's style. |

`WordPoolProvider` is what makes "any text" compose with "infinite mode": import a Rust manual
and race against an endless stream of Rust-manual vocabulary. Providers are seeded from a
recorded PRNG seed so any run can be reproduced exactly for replay and bug reports.

---

## 6. Typing rules

Configurable in `[typing]`:

| Setting | Values | Meaning |
|---|---|---|
| `stop_on_error` | `off` / `letter` / `word` | Whether a wrong keystroke blocks further input until corrected |
| `allow_backspace` | bool | Whether correction is possible at all |
| `strict_spaces` | bool | Whether a missing/extra space is an error or is silently absorbed |
| `space_advances_word` | bool | Whether space jumps to the next word even mid-word |
| `blind_mode` | bool | Hide correctness colouring until the run ends — trains accuracy without visual crutches |
| `confidence_mode` | `off` / `on` / `max` | `on` forbids backspacing over a completed word; `max` forbids backspace entirely |

Defaults: `stop_on_error = off`, `allow_backspace = true`, `strict_spaces = true`,
`space_advances_word = true`, `blind_mode = false`, `confidence_mode = off`.

Grapheme state machine, per target position:

```
Pending ──type correct──► Correct
   │                         │
   │                     backspace
   ├──type wrong────► Incorrect ──backspace──► Pending
   │                         │
   └──skipped (space)─► Missed
                             │
                     retype correct
                             ▼
                         Corrected     (counts as a first-attempt error,
                                        but as correct in the final text)
```

`Corrected` being distinct from `Correct` is what allows §4.2 to report both accuracy and
final correctness without ambiguity.

---

## 7. Progression and history

### 7.1 What is recorded

Every run, including abandoned ones (flagged `completed = 0` and excluded from personal
bests). Stored per run: the full metric set, the per-second timeline, per-key and per-bigram
deltas, error pairs, and — for races — the pacer curve.

### 7.2 What is shown

**Results screen (after every run)**
- Net / Gross / Raw WPM, accuracy, consistency, duration
- Per-second WPM line chart with the error markers overlaid, and the pacer curve for races
- Comparison against the player's average and personal best for that mode
- Top five error pairs and the slowest five bigrams
- Race: the speed wall, and a one-key jump to a drill

**History screen**
- WPM trend over time (sparkline / line chart), filterable by mode and date range
- Personal bests table, per mode and parameter
- Distribution histogram of WPM
- Current and longest daily streak, total time typed, total graphemes
- A key heatmap coloured by error rate over a chosen window
- Sessions list with drill-down to any individual run's results

### 7.3 Personal bests

Tracked per `(mode, parameter)` — a 15 s best and a 60 s best are separate records, because
they measure different things. A run qualifies only if `completed = 1` and accuracy ≥ 90%, so
a personal best cannot be set by typing nonsense quickly.

### 7.4 Streaks and goals

A configurable daily goal (default: 10 minutes or 5 runs). Streaks count consecutive days with
the goal met. Deliberately lightweight — no achievements, no levels, no confetti. The trend
line is the reward.

---

## 8. Session lifecycle

```
Menu
  │ pick mode + parameters + text
  ▼
Countdown (optional, configurable 0–5 s)
  │
  ▼
Ready ──first keystroke──► Running ──┬── timer expires ────► Finished
                              │      ├── text completed ───► Finished
                              │      ├── word count met ───► Finished
                              │      ├── pacer catches ────► Finished (race)
                              │      └── user quits ───────► Abandoned
                              │
                        [restart key] ──► new Ready
  ▼
Results ──┬── restart same config
          ├── new text, same config
          ├── drill the weak spots (race)
          └── back to menu
```

`Ready → Running` on the first keystroke is what makes the timer honest. `Abandoned` runs are
saved but never set records.

---

## 9. Default keybindings

Rebindable in `[keys]`; the defaults avoid the conflicts noted in
[review T4](CODEBASE_REVIEW.md#7-terminal-portability-gaps).

| Action | Default | Notes |
|---|---|---|
| Quit / back | `Esc` | Context-dependent: closes the top screen |
| Force quit | `Ctrl+Q` | From anywhere |
| Restart run | `Ctrl+R` | Same text |
| New text, same settings | `Ctrl+N` | |
| Menu during a run | `Esc` | Confirms if a run is in progress |
| History | `Ctrl+H` | |
| Text library | `Ctrl+L` | |
| Settings | `Ctrl+,` | Falls back to `F2` where the terminal cannot deliver it |
| Help / keys | `F1` or `?` | `?` only outside a run |

`Ctrl+T` is **not** used: it is `SIGINFO` on BSD, a tab key in several emulators, and collides
with common tmux prefixes. Keybindings are validated at config load, and bindings the terminal
cannot deliver are reported rather than silently ignored.
