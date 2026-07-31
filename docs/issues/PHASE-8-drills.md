# Phase 8 — Drills and progression

**Milestone:** `v2.0.0-rc.1` · **Issues:** TI-130 – TI-134 · **Goal:** close the learning loop.

Everything before this phase *measures*. This phase *acts on the measurement*: history becomes
targeted practice, so the app improves the typist rather than only scoring them. Content freeze
after this milestone — only fixes go into `v2.0.0`.

---

## TI-130 — Drill target selection

**Type** feat · **Size** M · **Priority** P1 · **Depends on** TI-041, TI-042, TI-058 · **Docs** [TECHNICAL §8.4](../TECHNICAL.md#84-drill-target-selection)

```
priority(target) = error_rate^1.5 × log(1 + attempts) × recency_weight
```

Each term earns its place: the `^1.5` exponent favours genuinely bad keys over marginally bad
ones; `log(1 + attempts)` suppresses targets with too little data to trust; `recency_weight`
decays over 30 days so a weakness that has since been fixed stops being drilled.

**Unit tests** (`DrillSelectionTest.cpp`)
- A key with a 50% error rate over 200 attempts outranks one with 100% over 2 attempts — the
  low-confidence suppression, tested directly.
- An old weakness that has been fixed ranks below a current one with the same lifetime error
  rate.
- Bigrams and single graphemes are ranked in one comparable ordering.
- With no history, selection returns a sensible default set rather than an empty one.
- With sparse history, it returns fewer targets rather than padding with noise.
- Ranking is deterministic; ties break by a documented rule, not by hash order.
- Weights and the decay window are configurable and honoured.
- Property: every returned target has a non-zero error count.

---

## TI-131 — Drill text synthesis

**Type** feat · **Size** M · **Priority** P1 · **Depends on** TI-130

Generate practice text concentrating the target pairs — real words containing them where
possible, generated sequences as a fallback.

**Unit tests** (`DrillSynthesisTest.cpp`)
- **Target bigram frequency in generated text is at least 5× its baseline frequency in ordinary
  text.** This is the measurable definition of "the drill actually drills the thing".
- Real words are preferred; the fallback triggers only when no word contains the target.
- Generated text is typeable: no character outside the target text's character set, no
  unreachable glyphs.
- Length matches the requested repetition count.
- The same seed reproduces the same drill.
- A target with no matching word still produces valid output.
- Non-ASCII targets (`č`, accented pairs) synthesise correctly.

**Acceptance**
- [ ] The 5× concentration property is asserted, not assumed.

---

## TI-132 — `DrillMode` and screen

**Type** feat · **Size** M · **Priority** P1 · **Depends on** TI-131, TI-043 · **Docs** [GAMEPLAY §2.6](../GAMEPLAY.md#26-drill)

**Unit tests** (`DrillModeTest.cpp`, `DrillScreenTest.cpp`)
- Finishes after the configured repetition count.
- Reports error rate **on the target keys specifically**, separately from overall accuracy —
  the whole point is to know whether the target improved.
- Results compare target-key error rate against the pre-drill baseline.
- Drill sessions persist with `mode = 'drill'` and their targets in `mode_param`, so progress on
  a specific weakness is queryable later.
- Selecting drill from the menu with no history offers the default set with an explanation.
- Snapshot at 80×24.

---

## TI-133 — Race-to-drill handoff

**Type** feat · **Size** S · **Priority** P2 · **Depends on** TI-128, TI-132 · **Docs** [GAMEPLAY §3.6](../GAMEPLAY.md#36-post-race-analysis--the-speed-wall)

One key from race results into a drill built from the speed wall's failing pairs. This closes
the loop the mode exists for: race → find the wall → drill the wall → race again.

**Unit tests** (`RaceToDrillTest.cpp`)
- The drill's targets are exactly the wall's top pairs, not the whole-run top pairs.
- A race with no identified wall offers the general drill instead, with the reason shown.
- Navigation returns correctly afterwards.

---

## TI-134 — Daily goal and streak UI

**Type** feat · **Size** S · **Priority** P3 · **Depends on** TI-107

**Unit tests** (`GoalUiTest.cpp`)
- Progress toward today's goal renders correctly at 0%, partial, met, and exceeded.
- The streak figure matches `HistoryService`.
- Goal type (time or run count) and target are configurable.
- Deliberately lightweight: no achievements, no levels, no confetti. The trend line is the
  reward, and the tests assert nothing more elaborate exists.

---

## Phase exit criteria

- [ ] The full loop works: race → speed wall → drill → measurable improvement in the target
      key's error rate across sessions.
- [ ] Drill text concentrates targets at ≥ 5× baseline.
- [ ] Selection ranks by confidence and recency, not by raw error rate alone.
- [ ] Drill sessions are queryable in history by target.
- [ ] **Content freeze.** Anything not merged by this tag goes to 2.1.
