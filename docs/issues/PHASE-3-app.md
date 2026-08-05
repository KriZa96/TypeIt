# Phase 3 — `typeit::app` and the CLI

**Milestone:** `v2.0.0-alpha.4` · **Issues:** TI-066 – TI-078 · **Goal:** use cases wired over
ports, and a headless binary that can run a complete typing session with no terminal.

The milestone that matters here: by the end of this phase **a full session runs, is measured,
and is persisted without a single line of FTXUI**. That is the proof the layering is real
rather than aspirational, and it means every bug found after this point is provably a UI bug.

---

## TI-066 — Scaffold `typeit_app` and declare the ports

**Type** build · **Size** S · **Priority** P0 · **Depends on** TI-023 · **Docs** [TECHNICAL §2.1](../TECHNICAL.md#21-ports)

**Scope**
- In: `libs/app` linking **only** `typeit::core`; the six port interfaces
  (`IHistoryRepository`, `ITextLibraryRepository`, `IConfigStore`, `IAssetLocator`,
  `IFileSystem`, plus `core::IClock` re-exported); `tests/app` target.
- Out: service implementations.

**Tests**
- Negative build test: including an `infra` or FTXUI header from `app` fails to compile.
- Every port has a virtual destructor (`static_assert`).

**Acceptance**
- [x] `app` links core only; `infra` is invisible to it. Enforced by
      `infra.lint.source_rules`, extended here to forbid `typeit/infra/` in `app` and both
      `typeit/infra/` and `typeit/app/` in `core` — watched to fail on a deliberate include in
      each direction.
- [x] Ports are pure interfaces with no data members: `is_a_port<T>()` static-asserts abstract,
      virtual destructor, and no state, for all six. Copying and moving are deleted — copying a
      port would copy an adapter's identity, a database connection or a file handle, into
      something that owns neither.
- [x] Taken out of order, ahead of the rest of Phase 3: TI-058 and TI-059 implement these
      interfaces and cannot be written until they exist.

---

## TI-067 — `ConfigService`

**Type** feat · **Size** S · **Priority** P0 · **Depends on** TI-066, TI-064

Load, validate, migrate, expose, and save configuration; notify observers on change.

**Unit tests** (`ConfigServiceTest.cpp`) — fakes only
- A load failure falls back to defaults **and surfaces the error to the caller** rather than
  swallowing it.
- A save failure is reported; the in-memory config is unchanged.
- Change notification fires once per save, with the new value.
- Validation runs before save; an invalid config is refused, not written.

---

## TI-068 — `SessionService`

**Type** feat · **Size** L · **Priority** P0 · **Depends on** TI-066, TI-064, TI-043 · **Docs** [ARCHITECTURE §5.2](../ARCHITECTURE.md#52-a-typing-run)

The central use case: resolve mode + text + provider, construct a `core::Session`, and on
finish compute metrics, persist the record, merge key/bigram stats, and update personal bests —
all in one transaction.

**Unit tests** (`SessionServiceTest.cpp`) — fake repositories and `FakeClock`
- A completed run persists exactly one session row, N sample rows, and merged key stats.
- **All persistence happens in a single transaction**: a failure at any step leaves the
  repository unchanged (injected failure at each step, table-driven).
- An abandoned run is stored with `completed = 0` and **does not** update personal bests.
- A run that qualifies sets the personal best; a worse run does not.
- The provider seed is recorded and reproduces the identical text stream.
- `app_version` is recorded from `Version.h`, never hardcoded.
- Starting a session with an invalid mode/text combination returns an error and persists
  nothing.
- Two sequential sessions do not share state — the regression guard for the deleted globals.

**Acceptance**
- [x] Failure at every persistence step is tested and leaves no partial write. The port grew
      `save_run(record, key_stats, error_map)` for this: `save` + `merge_key_stats` +
      `merge_error_map` are three transactions and cannot be made atomic from above, and a
      partial write is unrecoverable because merging the stats again double-counts the run
      that did land. The write failing outright is `SessionServiceTest`'s; a failure *after*
      the session row and the personal best is `HistoryRepositoryContract`'s, injected through
      the one mid-transaction failure the schema affords — `session_sample`'s
      `(session_id, t_ms)` primary key — and run against both the SQLite adapter and the fake.
- [x] No global state is read or written anywhere in the service. Both methods are `const`,
      and everything a finished run is filed under travels in the caller's `ActiveRun`, so two
      sessions in one process share nothing — asserted by `TwoSequentialSessionsShareNoState`.
- [x] `core::Session` was created here. ARCHITECTURE §5.2 specifies it and this issue says to
      construct one, but no issue ever added it. It is immovable and handed out through a
      `unique_ptr`, because `TypingModel` holds a span into the `TextBuffer` it owns.

---

## TI-069 — `HistoryService`

**Type** feat · **Size** M · **Priority** P1 · **Depends on** TI-068 · **Docs** [GAMEPLAY §7](../GAMEPLAY.md#7-progression-and-history)

Trends, aggregates, personal bests, streaks, filtering, and export.

**Unit tests** (`HistoryServiceTest.cpp`)
- Trend series is ordered and bucketed correctly by day/week.
- Streak counting: consecutive days, a broken streak, a same-day double session, and a
  timezone boundary (documented: local time).
- Daily-goal accounting against both the time and the run-count goal.
- Filters compose (mode + date range + limit).
- Export to CSV: correct header, correct escaping of quotes/commas/newlines in text titles.
- Export to JSON: valid JSON, round-trips through a parser, non-ASCII preserved.
- Empty history returns empty results, never NaN and never a crash.

---

## TI-070 — `TextLibraryService`

**Type** feat · **Size** M · **Priority** P1 · **Depends on** TI-071, TI-072, TI-064 · **Docs** [GAMEPLAY §5](../GAMEPLAY.md#5-text-supply--type-anything-you-want)

Import from file/stdin/paste, normalise, hash, deduplicate, score, tag, and bookmark.

**Unit tests** (`TextLibraryServiceTest.cpp`)
- Import stores both normalised and raw content.
- Re-importing identical content returns the existing id without a duplicate row.
- Re-importing the *same file* with changed content creates a new entry.
- Invalid UTF-8 is rejected with a byte offset and nothing is stored.
- An empty or whitespace-only file is rejected with a clear message — the successor to
  `is_file_valid`, now with the empty-file case that currently triggers
  [C1](../CODEBASE_REVIEW.md#4-correctness-defects) explicitly covered.
- A file exceeding the size limit is rejected with the limit named.
- A missing file returns `FileNotFound`, not a crash.
- Title defaults sensibly from the filename and is overridable.

---

## TI-071 — `core::TextNormalizer`

**Type** feat · **Size** M · **Priority** P1 · **Depends on** TI-030 · **Docs** [GAMEPLAY §5.2](../GAMEPLAY.md#52-normalisation-at-import)

Pure normalisation pipeline, in `core` because it is a domain rule, applied in the documented
order with each step individually toggleable.

**Unit tests** (`TextNormalizerTest.cpp`) — table-driven
- Line endings: CRLF → LF, lone CR → LF, mixed input.
- NFC normalisation: `e` + U+0301 → U+00E9.
- Typographic flattening: `" " ' '` → `" '`, en/em dash → `-`, `…` → `...`. **On by default**,
  because a typing test containing characters absent from the keyboard is unpassable.
- Whitespace collapsing; trailing whitespace stripped per line.
- Tab expansion at the configured width.
- Optional punctuation stripping and lowercasing.
- **Order matters**: a fixture proves the documented order produces a different result from a
  permuted order, so the order is pinned by test rather than by comment.
- **Idempotence**: normalising twice equals normalising once, for every combination of toggles.
- Every step individually disableable, verified by a toggle matrix.

**Acceptance**
- [x] Idempotence holds for all toggle combinations.
- [x] Pure: no I/O, no allocation beyond the result, same input → same output.

---

## TI-072 — Difficulty scoring

**Type** feat · **Size** S · **Priority** P2 · **Depends on** TI-030 · **Docs** [TECHNICAL §8.3](../TECHNICAL.md#83-difficulty-scoring-at-import)

Weighted score from mean word length, rare-word ratio, punctuation, capitals, digits, and
non-ASCII density, normalised to 1–10. Advisory only — it never gates anything.

**Unit tests** (`DifficultyScoreTest.cpp`)
- The bundled `simple.txt`, `medium.txt`, and `hard.txt` score in ascending order. This is a
  real, checkable property using the project's own corpora.
- Result is always within [1, 10].
- Empty text returns a defined value, not NaN.
- Weights sum to 1 (`static_assert` or a test).
- Scoring is deterministic.

---

## TI-073 — `ProfileService`

**Type** feat · **Size** S · **Priority** P1 · **Depends on** TI-069 · **Docs** [GAMEPLAY §3.4](../GAMEPLAY.md#34-starting-speed--where-progression-lives)

Adaptive baseline for race start speed, daily goal, streak accounting.

**Unit tests** (`ProfileServiceTest.cpp`)
- `V₀ = max(20, 0.85 × best_sustained_wpm over 30 days)`, verified on hand-built histories.
- No history → `V₀ = 20`.
- History older than 30 days is excluded; a run exactly at the boundary is handled per the
  documented rule.
- Only qualifying runs (completed, accuracy ≥ 90%) contribute.
- `α` and the floor are configurable and honoured.

---

## TI-074 — `typeit_cli` argument parser

**Type** feat · **Size** M · **Priority** P1 · **Depends on** TI-066 · **Docs** [TECHNICAL §7](../TECHNICAL.md#7-command-line-interface)

Hand-rolled parser (no dependency needed for this surface), producing a validated
`CliOptions` or a `Result` error.

**Unit tests** (`CliParserTest.cpp`) — table-driven over the full flag list
- Every flag in TECHNICAL §7 parses, long and short forms.
- `--` terminates option parsing.
- `-` means stdin.
- An unknown flag errors and **suggests the nearest valid flag** (edit distance).
- A missing required argument errors with the flag named.
- Mutually exclusive combinations (`--text` with `--text-id`) are rejected.
- Out-of-range numeric arguments are rejected with the valid range named.
- `--help` and `--version` short-circuit and exit 0.
- Argument order does not matter.

---

## TI-075 — `--simulate` headless harness

**Type** feat · **Size** M · **Priority** P0 · **Depends on** TI-068, TI-074 · **Docs** [TESTING §7](../TESTING.md#7-end-to-end-tests)

Drive a real session from a keystroke script with `FakeClock`, and print metrics as JSON. **No
terminal involved.** This is both the end-to-end test harness and the tool used later to
inspect race ramp curves without playing.

Script format (versioned per [VERSIONING §5](../VERSIONING.md#5-independent-version-numbers)):

```
# typeit-script 1
100  type h
250  type e
400  backspace
520  type e
```

**Unit tests** (`SimulateTest.cpp`, `ScriptParserTest.cpp`)
- Parser: valid scripts, comments, blank lines, unknown verbs, non-monotonic timestamps
  (rejected), missing version header, unsupported version.
- `type` accepts multi-byte graphemes.
- A full 30-second timed run produces metrics matching hand-computed expectations.
- Rerunning the same script yields byte-identical JSON output — the determinism property.
- Output JSON validates against a documented schema.

**Acceptance**
- [x] A complete session runs end to end with zero FTXUI symbols linked into the binary.
      `typeit::cli` links `typeit::app` and nothing else, and the layer check refuses FTXUI,
      SQLite, toml++ and `typeit/infra/` inside it. Only the repository and the clock are
      fakes in `SimulateTest`, and both are ports the real application injects too.
- [x] Determinism holds across runs and across platforms. The output carries **no wall-clock
      timestamp**: `started_at` and `ended_at` are omitted, and the timeline is rebased to
      zero — `core::timeline` stamps samples on the log's own clock, whose epoch is whenever
      the process started, so absolute values would have differed on every run. Asserted by
      running the same script twice with the clock moved a day in between and comparing the
      bytes.
- [x] The script format and the version-1 output schema are documented in
      [TESTING §7](../TESTING.md#7-end-to-end-tests).

---

## TI-076 — `--doctor`

**Type** feat · **Size** S · **Priority** P2 · **Depends on** TI-074, TI-055 · **Docs** [UX §6.2](../UX.md#62-capability-detection)

Report terminal capabilities, resolved paths, and database health. The first thing to ask a
user to run when they report a rendering problem in an unfamiliar terminal.

**Scope**
- In: detected colour depth and glyph set with the reason; resolved config/data/asset paths and
  whether each exists and is writable; database path, schema version, row counts, integrity
  check; version string.
- Out: the interactive key tester (Phase 9, TI-136 — it needs a terminal).

**Unit tests** (`DoctorTest.cpp`)
- Report is generated with a missing config, a missing database, and a healthy install.
- Detection reports the *reason* for its conclusion (which env var decided it).
- Output is stable and parseable.
- A corrupt database is reported as such, not crashed on.

**Acceptance**
- [x] Nothing in the report is an error. A missing database, an unwritable directory and a
      corrupt file are *findings*: a diagnostic that refuses to run because something is wrong
      is a diagnostic that is never there when it is needed.
- [x] Capability detection landed here rather than in Phase 4's TI-082, because "with the
      reason" is in this issue's scope and cannot wait for the TUI. It is
      `infra::detect_capabilities` — reading `COLORTERM` is I/O, and `cli` may not see `tui`.
- [x] Writability is established by writing a file and removing it, not by reading the
      permission bits: a directory can be mode 755 and still refuse a write over NFS, in a
      container, or on a full disk. The probe cleans up after itself, which is its own test.
- [x] `--doctor` on a missing database does not create one — checked, because a typo in
      `--data-dir` must not leave an empty database behind to confuse the next run.
- [x] Lives in `infra`: every fact in the report *is* an infra fact. Routing them through
      ports so a higher layer could format them would mean three new interfaces with one
      implementation each, for a diagnostic nobody tests through a fake. **Wiring the
      `--doctor` flag to it is the composition root's**, and there is no `main` yet — see the
      phase exit note.

---

## TI-077 — `--stats` and `--export`

**Type** feat · **Size** S · **Priority** P2 · **Depends on** TI-069, TI-074

**Unit tests** (`ExportCliTest.cpp`)
- `--stats` on an empty history prints a sensible message and exits 0.
- `--export csv` and `--export json` write valid, parseable output to stdout.
- `--last N` limits correctly; `N` larger than the history returns everything.
- Output is byte-identical across runs for the same database.
- Piping to a closed stdout does not crash (SIGPIPE handling).

---

## TI-078 — Version wiring end to end

**Type** feat · **Size** XS · **Priority** P1 · **Depends on** TI-002, TI-068, TI-074 · **Docs** [VERSIONING §6](../VERSIONING.md#6-single-source-of-truth)

**Scope**
- In: `--version` printing `kVersionString` plus git describe and build type;
  `session.app_version` populated from the header on every persisted run.
- Out: —

**Unit tests** (`VersionWiringTest.cpp`)
- `--version` output matches `kVersionString`.
- A persisted session row carries the current version.
- No literal version string exists outside `CMakeLists.txt` (grep test in CI).

---

## Phase exit criteria

- [ ] **`typeit --simulate script.tks` runs a complete session through the real stack — real
      services, real SQLite in a temp directory, real modes — with no terminal, and writes a
      correct row.** This is the phase's reason for existing.
- [ ] `--stats`, `--export csv`, `--export json`, `--doctor`, `--version` all work.
- [ ] Service tests run entirely on fakes and complete in under one second.
- [ ] Every service failure path is tested with an injected adapter failure.
- [ ] `app` coverage ≥ 85%.
- [ ] `app` links `core` only; the negative build test proves it.
- [ ] The legacy application still builds and passes its own tests.
