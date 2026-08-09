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
- [ ] The grace-window behaviour is tested at, just below, and just above the threshold.
- [ ] The convergence property passes end to end through `--simulate`.

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
- [ ] The progression property passes over a simulated multi-session history — this is the
      feature's whole justification, so it gets an explicit test rather than an assumption.

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
- [ ] The progression property (improving player → increasing start speed) passes.
- [ ] The pacer does not visibly stutter.
- [ ] Endless mode runs for 10 minutes with bounded memory.
- [ ] Race results identify the speed wall and its limiting key pairs.
- [ ] Tuning pass complete, with reasons recorded.
