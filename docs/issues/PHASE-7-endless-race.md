# Phase 7 — Endless and Race

**Milestone:** `v2.0.0-beta.2` · **Issues:** TI-120 – TI-129 · **Goal:** the progressive mode
that accelerates with the typist.

Scheduled late on purpose: race mode consumes history (adaptive start speed), the text library
(endless streams), and correct metrics (the accuracy gate). Building it earlier would mean
building it twice.

The controller is pure arithmetic, so almost all of it is unit-testable without a terminal —
which matters, because a difficulty ramp is exactly the kind of feature that is easy to get
subtly, unfalsifiably wrong.

---

## TI-120 — `EndlessMode`

**Type** feat · **Size** S · **Priority** P1 · **Depends on** TI-043, TI-117 · **Docs** [GAMEPLAY §2.5](../GAMEPLAY.md#25-endless)

Never finishes on its own; text streams forever; the HUD shows **rolling** WPM over the last 15
seconds rather than a cumulative average — in a run with no end, a cumulative average stops
responding to what you are doing now.

Implemented as race mode with the pacer disabled, not as a separate code path.

**Unit tests** (`EndlessModeTest.cpp`)
- Never reports finished regardless of duration or distance.
- The provider is pulled from as the cursor approaches the end of the buffered text, with no
  visible stall and no unbounded buffer growth.
- Rolling WPM reflects recent performance, not the whole run.
- Distance and peak rolling WPM are recorded.
- A 10-minute simulated run keeps memory bounded (the keystroke log grows linearly; nothing
  else does).

**Acceptance**
- [x] Built as race mode with the pacer disabled, not as a second code path — the only
      difference between "never finishes" and "finishes when the ghost catches you" is whether
      there is a ghost. Both register from the same resolved `RaceParams`, so a bad `[race]`
      preset is one message rather than two behaviours.
- [x] Never finished, however long the run and however far the typist gets.
- [x] The HUD number is **rolling** rather than cumulative, and a case proves the difference: a
      slow minute followed by fifteen fast seconds reads as fast. A cumulative average over an
      hour makes the last two minutes invisible, which is the one thing an endless run's typist
      wants to see.
- [x] Distance and peak rolling WPM are recorded — the two numbers an endless run *can* be
      scored on, since there is nothing to finish.
- [x] Ten simulated minutes with the rolling speed amortised O(1) per tick rather than a rescan
      of the log, and the accuracy window capped by construction.
- [ ] **The provider is not pulled from as the cursor nears the end.** See below.

### The refill needs a decision that is not this issue's to make

`Session::create` takes one chunk, once, and says why: the model holds a span into the buffer,
so appending would move the text out from under the cursor. Its comment defers the refill to
this phase, "where rebasing the model is the design rather than an afterthought".

It is not a Size S change. `TextBuffer` is immutable by design and stores a `vector<Grapheme>`;
`TypingModel` holds a `span` into it. Growing the buffer reallocates and dangles the span, so a
refill means rebuilding the model — and the model's per-position states and the keystroke log's
`target` indices are both relative to the buffer. Sliding a window over it invalidates every
logged index behind the window, and `Keystroke` does not store whether it was correct, so
metrics could no longer be recomputed for the part that scrolled away.

That is an architectural decision about how an endless run is *measured*, not a mode detail.
Worth noting while making it: a `Grapheme` is 16 bytes and a `Keystroke` is 32, so the buffer an
append-only refill would grow is **half the size of the keystroke log this issue already accepts
growing linearly**. Ten minutes at 80 WPM is about 64 kB of text against 128 kB of log. "Nothing
else grows" may be a stricter rule than the memory it is protecting.

---

## TI-121 — `Pacer`

**Type** feat · **Size** M · **Priority** P0 · **Depends on** TI-043 · **Docs** [GAMEPLAY §3.1](../GAMEPLAY.md#31-mechanics)

The ghost cursor. Position advances as `pacer += Δt · V · 5/60` graphemes.

**Unit tests** (`PacerTest.cpp`)
- Position after 60 s at 60 WPM is 300 graphemes (hand-verified against the five-graphemes-per-
  word convention).
- The grace period holds the pacer stationary for exactly the configured duration.
- A speed change mid-run applies from that moment, not retroactively.
- `push_back` places the pacer exactly `lead_comfort` behind the player.
- Fractional positions accumulate without drift over 10,000 small steps (accumulate in
  floating point, compare against a closed-form expectation).
- Zero speed does not advance; negative speed is rejected.
- Position never decreases except via `push_back`.

**Acceptance**
- [x] Every case above is a named test, plus the ones only writing it revealed.
- [x] **The first tick establishes the origin rather than covering the epoch.** A pacer handed a
      wall-clock timestamp would otherwise cover forty-five thousand years of graphemes on its
      first call — which is the same class of bug as 1.0 starting its clock at the menu.
- [x] Time going backwards advances nothing. A suspend or an NTP step must not teleport the
      pacer through the text and end a run the typist was winning.
- [x] The remainder of the step the grace period ends in still counts, so a race under a 60 Hz
      ticker starts in the same place as one under 30. Throwing it away would make the start
      depend on how often somebody happened to call `advance`.
- [x] A negative speed reads as stationary rather than as reverse. Nothing but the ramp law
      writes this and the ramp law clamps, so a negative is a caller's bug — and the safe
      reading of a bug is "does not move".
- [x] `push_back` near the beginning stops at the beginning rather than going negative, which
      would be an index every reader has to defend against for a case that means "the start".
- [x] The position is fractional and only rounded when somebody asks where to draw it. Rounding
      each step would lose a fraction of a grapheme per frame — at sixty frames a second, a
      pacer running slow by more than a word a minute.

---

## TI-122 — `DifficultyController` — the ramp law

**Type** feat · **Size** L · **Priority** P0 · **Depends on** TI-121 · **Docs** [GAMEPLAY §3.2](../GAMEPLAY.md#32-the-ramp-law)

The heart of the mode. Pure arithmetic, no I/O, no state beyond the parameters — which is what
makes the properties below testable at all.

```
r = +k_up · f(lead) · g(A)   if lead ≥ lead_comfort and A ≥ A_min
    0                        if lead_danger < lead < lead_comfort     (dead band)
    −k_down                  if lead ≤ lead_danger or A < A_min
```

**Unit tests** (`DifficultyControllerTest.cpp`) — the most important tests in the phase
- **Speed never rises while accuracy is below `A_min`, no matter how large the lead.** This is
  the accuracy gate `g(A)`, and it is the single term that makes the mode teach typing rather
  than teach mashing.
- Speed rises monotonically with lead above `lead_comfort`.
- `f(lead)` saturates at 1: a lead of 200 does not ramp faster than `lead_comfort + lead_scale`.
- **Dead band: speed is exactly unchanged for any lead strictly between `lead_danger` and
  `lead_comfort`.** This hysteresis is why the pacer does not stutter; without it `V`
  oscillates every tick around the boundary.
- Speed falls at `k_down` in the danger zone, independent of accuracy.
- Speed falls at `k_down` when accuracy drops below `A_min`, even with a comfortable lead.
- Clamped to `[V_min, V_max]` in both directions.
- **Recovery**: a stumble followed by restored lead and accuracy resumes climbing — no death
  spiral. Explicit scripted test.
- `k_down > k_up` for all three shipped presets (asserted on the preset table itself).
- Δt scaling: two half-steps equal one full step within tolerance.
- A full scripted run produces a speed curve matching a committed golden reference.

**Acceptance**
- [x] Every property above is a named test.
- [x] The controller has no dependency on time, I/O, or randomness — it takes Δt as a
      parameter. Which is what makes the rest of this list assertable at all: a ramp always
      produces *a* number, and a wrong one still looks like a race.
- [x] **No gain below `A_min`, asserted across the whole range of leads rather than at one
      convenient point** — twenty-eight combinations, up to a lead of ten thousand. Accuracy is
      tested first and alone in the law, so no lead however large can reach the climbing
      branch. This is the term that makes the mode teach typing rather than teach mashing.
- [x] The gate is proportional above itself rather than a switch, so a typist hovering on the
      boundary has no cliff to fall off.
- [x] The dead band leaves the speed **exactly** unchanged, swept across its whole width. Not
      nearly: without the hysteresis `V` oscillates every tick around the boundary and the
      typist watches the ghost twitch. The band is closed at the danger end and open at the
      comfort end, and both edges are asserted.
- [x] `f(lead)` saturates at one, so somebody two hundred graphemes ahead does not accelerate
      away from a speed they held only briefly.
- [x] Two half-steps equal one whole step, and a thousand ten-millisecond steps equal one
      ten-second step. The law is linear in Δt, so this is exact rather than close — and it is
      what makes the ramp independent of the frame rate it happens to be ticked at.
- [x] A stumble is recovered from rather than spiralled into, scripted rather than argued: five
      seconds of bad typing, then thirty of good, ending above where it started.
- [x] Parameters that would divide by zero have defined answers — a `lead_scale` of zero means
      any lead is full comfort, an `A_min` of one earns only on perfect. A NaN here would
      propagate into the speed, then the pacer, then every decision the run makes.
- [x] A full scripted run — climb, dead band, stumble, recovery — matches a committed golden
      curve, derived independently rather than read off the implementation. It is what TI-129's
      tuning pass regenerates, and what makes any constant change a diff somebody has to agree
      with.
- [x] The trend is exposed rather than inferred. A typist forty graphemes ahead and gaining
      nothing deserves to see *why*, and the controller is the only thing that knows which
      branch it took — without it the accuracy gate is invisible (TI-125).

---

## TI-123 — `RaceParams` and presets

**Type** feat · **Size** S · **Priority** P1 · **Depends on** TI-122 · **Docs** [GAMEPLAY §3.5](../GAMEPLAY.md#35-presets)

Gentle / Standard / Brutal, plus `custom`, with every value overridable in `[race]`.

**Unit tests** (`RaceParamsTest.cpp`)
- Each preset loads with the exact values in the GAMEPLAY §3.5 table (table-driven against the
  document).
- `preset = "custom"` uses the explicit config values.
- Cross-validation: `lead_danger < lead_comfort`, `min_accuracy ∈ (0,1)`, `ramp_up > 0`,
  `lives ≥ 1`, `V_min < V_max`.
- An invalid combination is rejected with a message naming the offending relationship.
- Ordering property: Gentle ramps slower and forgives more than Standard, which does likewise
  versus Brutal — asserted on the parameters, so a future edit to the table cannot silently
  invert the difficulty ordering.

**Acceptance**
- [x] Each preset holds exactly the values in the GAMEPLAY §3.5 column that explains it, so the
      table and the document cannot drift apart.
- [x] **The ordering is asserted on the parameters across seven axes** — ramp up, ramp down,
      accuracy gate, grace, lives, start factor, comfort lead. A "Gentle" that ramps faster than
      "Brutal" is a bug nobody would ever report, because both still produce a working race.
- [x] `k_down ≥ k_up` in every preset, which is what makes a stumble breathing room rather than
      the start of a slide. Brutal is the equality case, deliberately.
- [x] A named preset uses the table and **ignores** the explicit values. Otherwise a stale
      `ramp_up` left in a file from an afternoon of experimenting follows somebody into every
      preset they pick afterwards, and "standard" stops meaning the same thing in two config
      files. Only `custom` reads them.
- [x] A misspelled preset is named in the error rather than falling back to Standard — a typo
      in a difficulty is somebody playing the wrong game and wondering why.
- [x] The cross-validation is on `RaceParams`, not on `RaceConfig`. A preset, a command line and
      a settings screen can all produce a parameter set, and only one of those three is a file;
      the per-field ranges stay where they were, in the config validator.
- [x] `catch_penalty` and `start_factor` are in `[race]` now. GAMEPLAY §3.5 says every value is
      overridable and these two were the ones that were not.
- [x] `custom` is not a preset. It is a real configuration value and the *absence* of a
      difficulty, so the two questions stay apart in the type.

---

## TI-124 — `RaceMode`

**Type** feat · **Size** L · **Priority** P0 · **Depends on** TI-122, TI-123 · **Docs** [GAMEPLAY §3.3](../GAMEPLAY.md#33-failure-and-grace)

Lives, grace window, catch detection, push-back, and termination.

**Unit tests** (`RaceModeTest.cpp`) — with `FakeClock`, fully deterministic
- The run ends when `lead ≤ 0` continuously for `grace_ms`.
- **A momentary `lead ≤ 0` shorter than `grace_ms` does not end the run** — a single fumbled
  keystroke must not be fatal; a sustained inability to keep up must be.
- The grace timer resets when the player regains a positive lead.
- With `lives > 1`: being caught consumes a life, pushes the pacer back to `lead_comfort`,
  applies `catch_penalty` to the speed, and continues.
- Reaching zero lives ends the run.
- The 5-second start grace holds the pacer stationary.
- The mode records `peak_wpm` as the highest speed sustained for ≥ `sustain_window_s` — **not**
  the peak instantaneous value, which any lucky burst would inflate.
- Quitting mid-race records an abandoned run.
- Property: a scripted player typing at a constant 60 WPM with 99% accuracy converges to a
  pacer speed near 60 and is caught only after a sustained slowdown.

**Acceptance**
- [x] The grace-window behaviour is tested at, just below, and just above the threshold — the
      one duration that decides whether a fumble is survivable, so it is asserted on both sides
      of itself rather than somewhere comfortably either side.
- [x] Nothing at all happens during the opening grace, catching included. The typist has typed
      nothing and the lead is zero, which is *exactly* the condition for being caught; a grace
      that held only the pacer still ended the run 300 ms into the five seconds meant for
      reading the first line. Found by test, not by reasoning.
- [x] Regaining a lead resets the grace timer, asserted over twenty stumbles — otherwise a race
      is lost by accumulating 300 ms of fumbles across ten minutes, which is every race.
- [x] A backspace is not an attempt, so the accuracy gate cannot be moved by deleting rather
      than by typing. `Corrected` counts as a miss, as first-attempt accuracy does everywhere
      else in this project.
- [x] Nothing typed yet reads as full accuracy rather than none. Opening at zero would have the
      gate back the speed off before the first key is pressed.
- [x] Being caught with lives left pushes the ghost back, applies the penalty and refreshes the
      displayed lead in the same breath — the HUD should show the ghost where the push-back put
      it, not where it was a moment before somebody lost a life.
- [ ] **The convergence property does not hold, and cannot with the law as documented.**
      See below; this box stays open on purpose.

### The convergence property contradicts the ramp law

This issue asks for "a scripted player typing at a constant 60 WPM with 99% accuracy converges
to a pacer speed near 60 and is caught only after a sustained slowdown". [GAMEPLAY
§3.2](../GAMEPLAY.md#32-the-ramp-law) specifies a ramp that reads the **lead** and not the rate
the lead is changing at. Both cannot be true.

The lead is the integral of the speed difference, so by the time the ghost is faster than the
typist there is a large accumulated lead still to burn off — and the ramp keeps climbing for the
whole of it, because `f(lead)` is saturated the entire time. That is integrator windup, and the
overshoot is **structural rather than a matter of constants**: no value of `k_up` or `k_down`
removes it.

Measured, from a simulation of §3.2 written independently of the implementation and then
confirmed against it:

| Typist | Start | Peak pacer speed | Caught at |
|---|---|---|---|
| 60 WPM | 30 | 90 | 2 min 8 s |
| 60 WPM | 20 | 100 | 2 min 35 s |
| 90 WPM | 30 | 150 | 3 min 35 s |
| 40 WPM | 20 | 60 | 1 min 52 s |

The mode still works, and arguably still does what §3 asks of it — "keep the player in the band
just above their sustainable speed **for as long as possible**" is a mode that ends. But it ends
for a typist who never slowed down, at a pacer speed half again their own, which is not what
TI-124 predicted.

`RaceModeTest` asserts what the law measurably does rather than what the issue hoped, so a
change here surfaces as a decision somebody made. Resolving it needs a term the law does not
have — the simplest being to stop climbing while the lead is *shrinking*, which is one
comparison and one stored value. That is a design decision rather than a tuning one, so it is
recorded here for TI-129 rather than taken unilaterally.

---

## TI-125 — Race HUD

**Type** feat · **Size** M · **Priority** P1 · **Depends on** TI-124, TI-087 · **Docs** [UX §3.3](../UX.md#33-session--race)

Pacer bar, inline pacer marker in the text, target speed with a trend indicator, lead, and
lives.

**Unit tests** (`RaceHudTest.cpp`)
- The pacer bar's filled proportion matches the pacer position; the caret marks the player, and
  the gap between them is the lead.
- The inline marker lands on the correct grapheme, including after wide characters.
- The trend indicator shows `▲` while climbing, `▼` while backing off, `=` in the dead band —
  so the accuracy gate is *visible* rather than mysterious. A player who is 40 graphemes ahead
  and not gaining speed should be able to see why.
- Lives render, and update on being caught.
- ASCII fallback for every race glyph.
- Layout holds when the pacer is ahead of the player (negative lead).
- Snapshots at 80×24 and 120×40.

**Acceptance**
- [x] The gap between the filled run and the caret **is** the lead, drawn to scale — asserted by
      comparing two leads rather than by eyeballing one, because a bar that looks right and
      measures wrong lies to somebody mid-race.
- [x] The bar spans a **window** of the text rather than all of it. Over a three-thousand
      grapheme chapter a lead of thirty is a fifth of a column, which is not a feedback channel;
      four times `lead_comfort` makes a comfortable lead about a quarter of the bar.
- [x] The trend marker is the accuracy gate made visible — `▲`/`▼`/`=` beside the target speed,
      and coloured as well as shaped, so somebody who is well ahead and gaining nothing can see
      *why* rather than wonder why the number stopped moving.
- [x] The layout holds when the ghost is ahead: a negative lead is an ordinary state for the
      length of the grace window, and the bar keeps both its ends. The lead reads `-12` rather
      than `12`, which would be a lie about which way round the two are.
- [x] Lives show what is left **and** what has gone, so the total is visible rather than
      remembered.
- [x] Every race glyph has an ASCII form, asserted by checking that nothing in either strip has
      the high bit set under `GlyphSet::Ascii`.
- [x] A bar narrower than its own two ends draws nothing rather than a partial one — the
      responsive layout has a screen for that case.
- [x] Snapshots at 80×24 and 120×40, plus the render-purity assertion every widget owes.
- [x] **It is actually on screen.** The session screen draws it for a race and not for anything
      else, with a test that says so — a HUD nobody draws is the same class of thing as a flag
      that reports a feature is missing when it is there.
- [ ] The inline pacer marker in the text is not drawn. `TypingArea` owns the text and takes no
      pacer position; threading one through is a change to the widget that every other mode
      also uses, and the bar already carries the same information.

---

## TI-126 — Adaptive start speed

**Type** feat · **Size** S · **Priority** P0 · **Depends on** TI-073, TI-124 · **Docs** [GAMEPLAY §3.4](../GAMEPLAY.md#34-starting-speed--where-progression-lives)

`V₀ = max(20, 0.85 × best_sustained_wpm over 30 days)`. This is where progression actually
lives: today's ceiling becomes tomorrow's floor, so the player never re-grinds speeds they have
already proven.

**Unit tests** (`AdaptiveStartTest.cpp`)
- First-ever race starts at 20 WPM.
- With a 90 WPM sustained best, the next race starts at 76.5.
- History older than 30 days is excluded.
- Only qualifying runs (completed, accuracy ≥ 90%) contribute.
- `start_policy = "fixed"` uses `start_wpm` and ignores history entirely.
- `α` and the floor are configurable and honoured.
- **Progression property**: a sequence of simulated races with improving performance produces a
  strictly increasing `V₀`.

**Acceptance**
- [x] The progression property passes over a simulated multi-session history — this is the
      feature's whole justification, so it gets an explicit test rather than an assumption.
      Twelve sessions, each better than the last, each starting strictly above the one before.
- [x] **Only runs typed well enough count.** The gap this issue closed: the ramp will happily
      push the pacer to 120 while somebody mashes at 60% accuracy, and starting tomorrow's race
      there would mean losing immediately, every time, forever. The bar travels to the
      repository as a parameter beside the window, because "which runs count" is a domain rule
      and infra's job is the maximum.
- [x] A bad day does not undo weeks of progress: it is the *best* of the window, not the last
      of it. Somebody tired on a Tuesday has not become a slower typist.
- [x] A plateau stops rising rather than drifting, which is the other half of the same
      property.
- [x] `start_policy = "fixed"` never asks the history at all — asserted with the repository
      armed to fail, so a fixed start survives a database that cannot be read.
- [x] **The composition root actually uses it.** Registering the race mode with
      `config.race.start_wpm` was the state before this issue: the formula existed, was tested,
      and nothing called it. A history that cannot be read reports and falls back to the
      configured speed rather than silently dropping somebody to 20 WPM.

---

## TI-127 — Race persistence

**Type** feat · **Size** S · **Priority** P1 · **Depends on** TI-124, TI-058

`peak_wpm`, `wall_wpm`, the pacer curve in `session_sample`, race parameters in `mode_param`,
and race personal bests.

**Unit tests** (`RacePersistenceTest.cpp`)
- `peak_wpm` and `wall_wpm` round-trip.
- `pacer_wpm` samples are stored per second and reload correctly.
- `mode_param` JSON captures the preset and every effective parameter, so a past race is fully
  reconstructible even after the config changes.
- Race personal bests are tracked on `peak_wpm`, separately from timed-mode bests on `net_wpm`.
- An abandoned race does not set a best.

**Acceptance**
- [x] `peak_wpm` and `wall_wpm` are filled by the service and round-trip. They were declared on
      the record and on the schema and nobody wrote them — the mode knew both and was never
      asked.
- [x] **The sustained peak is the level the speed never dropped below for a whole window**, not
      the same number for ten seconds. A ramp that moves continuously never produces the latter,
      and asking for it recorded a peak of zero for every race ever run. Found by writing this
      issue's first test; it is the number that becomes the next race's starting speed, so it
      being silently zero would have made TI-126 a no-op too.
- [x] No collapse means **no** wall rather than a wall of zero — a mark on a chart saying
      nothing happened is worse than no mark.
- [x] `pacer_wpm` is written and read. The column shipped in schema v1 and nothing had ever put
      a value in it.
- [x] The ghost's curve is sampled once a second, not once a tick: at sixty frames a second an
      hour's race would be a quarter of a million rows nobody plots, and the speed changes by
      less than a word a minute between samples.
- [x] `mode_param` records every effective parameter, so a race stays reconstructible after
      somebody edits a preset. Reading the config back at display time would mean last week's
      race quietly re-describing itself as this week's difficulty.
- [x] A `mode_param` the caller supplied is left alone — the caller knows which preset was
      chosen *by name*, which this does not.
- [x] A mode that is not a race records neither number and no pacer, so a timed run does not
      carry two empty columns and a chart with an empty second series.
- [x] Race bests are on `peak_wpm` and timed bests on `net_wpm`, already separate per metric;
      an abandoned race sets none, which the completed guard already enforced and a test now
      says out loud.

---

## TI-128 — Speed-wall analysis

**Type** feat · **Size** M · **Priority** P1 · **Depends on** TI-127, TI-105 · **Docs** [GAMEPLAY §3.6](../GAMEPLAY.md#36-post-race-analysis--the-speed-wall)

The WPM band where rolling accuracy first fell below `A_min` and did not recover, plus the five
grapheme pairs that failed most often **inside that band** — which are, by construction, the
keys actually limiting the player's speed, as opposed to the ones they get wrong when typing
comfortably.

**Unit tests** (`SpeedWallTest.cpp`)
- The wall is identified correctly in a scripted run with a known collapse point.
- A run with no accuracy collapse reports no wall, rather than inventing one.
- A transient dip that recovers is **not** reported as the wall.
- Error pairs are filtered to the band, and differ from the whole-run top five in a fixture
  designed to make them differ — otherwise the feature is doing nothing.
- Sparse data in the band degrades gracefully.

**Acceptance**
- [x] **Computed after the run, not during it.** Live, "accuracy has dropped" and "accuracy has
      dropped and will recover in two seconds" are indistinguishable, and only one of them is a
      wall. TI-127's first cut recorded the speed at the first dip, which is precisely the
      transient this issue says must not count; that field is gone.
- [x] A transient dip that recovers is not the wall, which is what the whole thing turns on.
      Found by scanning **backwards** for the last acceptable moment rather than forwards for
      the first bad one — "and did not recover" becomes one comparison instead of a search.
- [x] A run with no collapse reports no wall rather than inventing one. Being overtaken while
      typing well is not a wall, and a diagnosis for it would be a made-up one.
- [x] A run that was never accurate walls from its first keystroke, rather than from nowhere.
- [x] **The pairs differ from the whole run's**, on a fixture built to make them differ — an
      early repeated mistake that recovers, and a different mistake in the collapse. A feature
      that returns the same answer as the cheap version is a feature doing nothing.
- [x] First-attempt accuracy, matching what the gate read during the race, so the analysis
      afterwards agrees with what the ramp was reacting to at the time.
- [x] Sparse data degrades: a collapse the curve says nothing about — a race that ended inside
      one second — reports the pairs without inventing a band. What failed is still known even
      when the speed is not.
- [x] At most five pairs, most frequent first, ties broken by the pair itself so two machines
      agree. It is a list somebody reads before a drill, not a dataset.

---

## TI-129 — Ramp tuning pass

**Type** chore · **Size** M · **Priority** P1 · **Depends on** TI-124 – TI-128 · **Docs** [ROADMAP risks](../ROADMAP.md#what-could-go-wrong)

The constants in GAMEPLAY §3.5 are informed guesses until someone plays it. This is the
explicit task that turns them into measured values, and it exists because "tune it later"
otherwise never happens.

**Scope**
- In: use `--simulate` with scripted players at 30/50/70/90 WPM and varying accuracy to plot
  speed curves for all three presets; play each preset for real; adjust constants; record the
  before/after reasoning in the issue.
- Out: adding new parameters — tune what exists.

**Tests**
- Golden speed curves regenerated and committed after tuning, with the diff reviewed.
- All TI-122 properties still hold after any constant change — the properties are invariants,
  the constants are not.

**Acceptance**
- [ ] Every changed constant has a recorded reason.
- [ ] Each preset is played for at least 20 real runs before the issue closes.
- [ ] Gentle is genuinely gentle and Brutal is genuinely brutal, confirmed by play rather than
      by the parameter table.

---

## Phase exit criteria

- [ ] All `DifficultyController` properties pass: no gain below `A_min`, dead-band hysteresis,
      monotone response to lead, recovery after a stumble.
- [ ] The convergence property (constant-rate player → matching pacer speed) passes end to end.
      **Blocked on a contradiction in the specification** — see TI-124. The documented ramp law
      overshoots by construction, and the property cannot hold without a term the law does not
      have.
- [x] The progression property (improving player → increasing start speed) passes.
- [ ] The pacer does not visibly stutter.
- [ ] Endless mode runs for 10 minutes with bounded memory. The mode's own state is bounded and
      tested; the text buffer is not refilled at all yet — see TI-120.
- [x] Race results identify the speed wall and its limiting key pairs. Showing them on the
      results screen, and the one-key jump into a drill, are TI-125's and Phase 8's.
- [ ] Tuning pass complete, with reasons recorded.
