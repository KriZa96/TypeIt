# TypeIt — Documentation

Planning and design documentation for rebuilding TypeIt into a full application.

*Status: planning complete, implementation not started. Written 2026-07-30 against commit
`d7a2d1c`.*

---

## Read in this order

| # | Document | What it answers |
|---|---|---|
| 1 | [CODEBASE_REVIEW.md](CODEBASE_REVIEW.md) | What the current code does well, what is broken, and why a migration rather than a rewrite |
| 2 | [ARCHITECTURE.md](ARCHITECTURE.md) | The layered design, the dependency rule, and twelve ADRs with their costs |
| 3 | [GAMEPLAY.md](GAMEPLAY.md) | Every mode, the Race ramp law, and the exact definition of every metric |
| 4 | [TECHNICAL.md](TECHNICAL.md) | Key types, algorithms, the SQLite schema, the config format, the CLI |
| 5 | [UX.md](UX.md) | Screen designs, theming, terminal compatibility, what is and is not customisable |
| 6 | [TEXT_SOURCES.md](TEXT_SOURCES.md) | Ingestion pipeline: files, code, subtitles, ebooks, web pages, external converters |
| 7 | [STYLE.md](STYLE.md) | Naming, ownership, error handling, comments, commits |
| 8 | [BUILD.md](BUILD.md) | CMake structure, dependency strategy, presets, packaging |
| 9 | [CI_CD.md](CI_CD.md) | The full GitHub Actions design — **built first** |
| 10 | [TESTING.md](TESTING.md) | Test levels, property tests, and which current tests carry over |
| 10a | [LEGACY_TEST_AUDIT.md](LEGACY_TEST_AUDIT.md) | All 64 legacy tests, one row each: ported, superseded, dropped or deferred |
| 11 | [VERSIONING.md](VERSIONING.md) | SemVer policy, how to version properly, pipeline automation |
| 12 | [ROADMAP.md](ROADMAP.md) | Phases with acceptance criteria and risks |
| 13 | [DIAGRAMS.md](DIAGRAMS.md) | 30 diagrams — class, sequence, state, ER, flow, pipeline |
| 14 | [issues/](issues/README.md) | **171 issues**, task by task, with tests and acceptance criteria |

**Want the shape of it quickly?** [DIAGRAMS.md](DIAGRAMS.md) is the visual index — layering,
domain classes, the typing-run sequence, the race state machine, the database, and the pipeline.

Day-to-day, work from [issues/README.md](issues/README.md) — it carries the full backlog, the
definition of done, and the critical path.

---

## The plan in one page

**What it becomes.** A terminal typing trainer where you can type any text you supply, with a
persistent history and real metrics, deep customisation of everything a terminal can control,
and a progressive Race mode whose pacer accelerates as you improve and whose starting speed is
derived from your own recorded history.

**How it is built.** Ports and adapters, four libraries with a strictly one-directional
dependency rule enforced by CMake targets rather than by review:

```
apps/typeit  →  tui / cli  →  app  →  core        (infra implements app's ports)
```

`typeit::core` links against the standard library and nothing else. That single constraint is
what makes the domain testable headlessly, resize-safe, and replayable.

**The four decisions that shape everything else**

| Decision | Consequence |
|---|---|
| The keystroke log is the source of truth; metrics are derived (ADR-002) | The accuracy-after-backspace defect becomes unrepresentable, and per-second charts, replay, and offline recomputation come free |
| Rendering is a pure function of state (ADR-003) | Game logic stops running inside draw calls ten times a second |
| Text is grapheme clusters, not bytes (ADR-004) | "Type any text you want" actually includes ćčšđž, CJK, and emoji |
| No global mutable state (ADR-005) | Tests stop being order-dependent; headless simulation becomes possible |

**Settled by choice**

- Dependencies: `FetchContent` with a `find_package` fallback. The vcpkg submodule is removed;
  `vcpkg.json` stays as an opt-in path.
- History: SQLite. Config: TOML. Both at OS-standard paths, both behind ports.
- Migration: strangler on a branch — the new core is built alongside the old app, and the old
  code is deleted in one reviewable step once the new frontend reaches parity.
- **Font family and size are out of scope.** A terminal application cannot set them; TypeIt
  will not ship a setting that does not work, and no GUI frontend is planned. Everything else
  — themes, colour depth, glyph sets, caret, layout, HUD, typing rules, keybindings, text
  pipeline, race difficulty — is customisable. See [UX §1](UX.md#1-what-is-and-is-not-customisable).

**Order of work.** **CI/CD pipeline** → build system → domain → persistence → services → new
frontend and cutover → history screens → text library and extended sources → endless and race →
drills → release. Detail and acceptance criteria in [ROADMAP.md](ROADMAP.md); the executable
version is [the issue backlog](issues/README.md).

The pipeline is built **first**, against the codebase as it stands today, so the rebuild is
never written without a net. The current repository has one CI job — Linux, Debug, gcc — which
is why it can contain three `-Wreorder` violations, undefined behaviour on an empty file, and
Windows build instructions no machine has ever executed. See [CI_CD.md](CI_CD.md).

**Versioning.** Today's application is `1.0.0`, tagged retroactively so the starting point stays
checkoutable. The rebuild is `2.0.0` — a MAJOR bump because metric definitions, keybindings, and
data locations all change. Each phase ends with a pre-release tag
(`2.0.0-alpha.1` … `2.0.0-rc.1`). 1.0.0 persists no user data, so there is no migration burden
and 2.0.0 gets to choose its formats freely. See [VERSIONING.md](VERSIONING.md).

---

## Conventions used here

- Findings in the review are labelled `C1`–`C9` (correctness), `D1`–`D11` (build/tooling),
  `T1`–`T6` (terminal portability); other documents cite them by label.
- Architecture decisions are `ADR-001`–`ADR-012`; other documents cite them by number.
- Every design choice states its cost. A decision without a stated cost has not been thought
  through.
