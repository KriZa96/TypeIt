# TypeIt — Versioning Policy

*How versions are assigned, where the number lives, and what may break when.*

**Looking for the practical guide?** §11 is the decision procedure — how to actually pick a
number, with worked examples from this project. §12 covers what the pipeline automates and what
it deliberately leaves to a human.

---

## 1. Scheme

TypeIt follows [Semantic Versioning 2.0.0](https://semver.org): `MAJOR.MINOR.PATCH`, with
optional pre-release and build metadata.

| Component | Increments when |
|---|---|
| **MAJOR** | Anything user-visible breaks: a removed CLI flag, a removed config key, a changed metric definition, a data location move, or a database that older versions cannot read |
| **MINOR** | New functionality, backwards compatible: a new mode, a new screen, a new config key with a default, a new CLI flag |
| **PATCH** | Bug fixes and internal work with no interface change |

---

## 2. The two versions that exist

### `1.0.0` — the current application

Today's FTXUI application as it stands at commit `d7a2d1c`. It is tagged retroactively, with
no code change, so that the starting point of the rebuild is a named, checkoutable thing.

```bash
git tag -a v1.0.0 d7a2d1c -m "TypeIt 1.0.0 — original FTXUI typing test"
```

Capabilities of 1.0.0, for the record: three bundled difficulties plus a custom file path,
15/30/60/custom timers, live WPM and accuracy, per-character colour feedback, restart, and
exit to menu. **It persists nothing.**

### `2.0.0` — the rebuild

Everything described in [ARCHITECTURE.md](ARCHITECTURE.md) through
[ROADMAP.md](ROADMAP.md). It is a MAJOR bump because essentially every user-visible contract
changes:

- Metric definitions change ([GAMEPLAY §4](GAMEPLAY.md#4-metrics--exact-definitions)) — a WPM
  number from 1.0.0 is not comparable to one from 2.0.0. This alone forces MAJOR.
- Keybindings change (`Ctrl+T` is gone).
- Configuration, a database, and OS-standard data directories appear where there were none.
- A CLI appears.
- Assets move out of the source tree.

**There is no data migration from 1.0.0, and none is needed** — 1.0.0 stores no user data at
all. This is the one respect in which the rewrite is unusually cheap, and it is worth
exploiting: 2.0.0 gets to choose its formats freely, with no legacy shape to accommodate.

---

## 3. Pre-releases during the rebuild

Each phase in [ROADMAP.md](ROADMAP.md) ends with a tagged pre-release. Pre-release identifiers
sort below the final release (`2.0.0-alpha.1 < 2.0.0-beta.1 < 2.0.0-rc.1 < 2.0.0`), so the
ordering is meaningful to tooling and to humans.

| Tag | Ends phase | What it means |
|---|---|---|
| `v2.0.0-alpha.1` | 0 — Foundation | Builds clean on Linux and Windows with warnings as errors. Behaviour unchanged from 1.0.0 |
| `v2.0.0-alpha.2` | 1 — Core | Domain complete and headless. No user-visible change yet |
| `v2.0.0-alpha.3` | 2 — Infra | Persistence and configuration exist |
| `v2.0.0-alpha.4` | 3 — App | A full session runs headless via `--simulate` |
| `v2.0.0-alpha.5` | 4 — TUI + cutover | **Parity reached; the 1.0.0 code is deleted.** First tag that is a usable application again |
| `v2.0.0-alpha.6` | 5 — History | History and analytics screens |
| `v2.0.0-beta.1` | 6 — Text library | Feature-complete for "type any text". First tag worth using daily |
| `v2.0.0-beta.2` | 7 — Endless + Race | Feature-complete overall |
| `v2.0.0-rc.1` | 8 — Drills | Content freeze; only fixes after this |
| `v2.0.0` | 9 — Release | Packaged and released |

Alpha tags are for the developer. Beta tags are usable. Only `rc` and the final release are
announced.

---

## 4. After 2.0.0

Planned, not promised:

| Version | Theme |
|---|---|
| `2.1.0` | Extended text sources: EPUB, the external converter hook, web import ([Phase 6A](issues/PHASE-6A-text-sources.md) waves 3–4) |
| `2.2.0` | Multi-profile support; per-profile history |
| `2.3.0` | Layout/keyboard-aware analysis (finger and row statistics); alternative layouts |
| `3.0.0` | Reserved. Only if something genuinely has to break |

Note that every one of these is MINOR: new capability, nothing existing changes. That is the
shape a healthy release line should have — see §11.3 for why the accuracy fix in 2.0.0 was the
exception rather than the rule.

Patch releases go out as needed. There is no release schedule and no obligation to invent one.

---

## 5. Independent version numbers

The application version is not the only version, and coupling them would be a mistake — a
schema change must not force a MAJOR bump, and a MAJOR bump must not imply a migration.

| Stream | Type | Location | Rule |
|---|---|---|---|
| **Application** | SemVer | `project(TypeIt VERSION …)` | This document |
| **Database schema** | Monotonic integer | `PRAGMA user_version` | +1 per migration, never reused, never decremented. A database with a *higher* version than the binary understands is refused with a clear message, not upgraded or wiped |
| **Config file** | Monotonic integer | `config_version` key | +1 when a key is renamed or its meaning changes. Migration is automatic and writes a backup first |
| **Theme format** | Monotonic integer | `theme_version` key | +1 on a breaking format change. Unknown keys are ignored so most theme changes need no bump |
| **Keystroke script** | Monotonic integer | header line | +1 on format change. Used by `--simulate` and by test fixtures |

2.0.0 ships with schema version 1, config version 1, theme version 1.

---

## 6. Single source of truth

The version is declared **once**, in the top-level `CMakeLists.txt`:

```cmake
project(TypeIt VERSION 2.0.0 LANGUAGES CXX)
set(TYPEIT_VERSION_PRERELEASE "alpha.1" CACHE STRING "SemVer pre-release identifier, or empty")
```

`configure_file` generates `typeit/core/Version.h`:

```cpp
namespace typeit {
inline constexpr int         kVersionMajor = @PROJECT_VERSION_MAJOR@;
inline constexpr int         kVersionMinor = @PROJECT_VERSION_MINOR@;
inline constexpr int         kVersionPatch = @PROJECT_VERSION_PATCH@;
inline constexpr const char* kVersionPrerelease = "@TYPEIT_VERSION_PRERELEASE@";
inline constexpr const char* kVersionString = "@TYPEIT_VERSION_FULL@";   // "2.0.0-alpha.1"
inline constexpr const char* kGitDescribe   = "@TYPEIT_GIT_DESCRIBE@";   // "v2.0.0-alpha.1-3-gabc1234"
}
```

Every consumer reads from there:

- `typeit --version`
- The help screen footer
- `session.app_version` on every recorded run — so a metric-definition change is
  traceable in history, and old rows can be identified and excluded from comparisons
- CPack package filenames
- The log file header

**Nothing hardcodes a version string anywhere else.** A grep for a literal version number
outside `CMakeLists.txt` is a review failure.

`TYPEIT_GIT_DESCRIBE` is captured at configure time and is empty in a source tarball; it is
diagnostic only and never used for logic.

---

## 7. Git conventions

| | |
|---|---|
| Release tags | `vMAJOR.MINOR.PATCH[-prerelease]`, annotated, e.g. `v2.0.0-alpha.1` |
| Default branch | `main` — always builds, always green, always releasable |
| Work branches | `feat/TI-042-typing-model`, `fix/TI-005-empty-file`, `refactor/…`, `build/…` |
| Rebuild integration branch | `v2` — phases merge here; `v2` merges to `main` at the Phase 4 cutover |

Every commit on `main` builds with `TYPEIT_WERROR=ON` on gcc, clang, and MSVC, and passes the
full test suite.

---

## 8. CHANGELOG

`CHANGELOG.md` at the repository root, in [Keep a Changelog](https://keepachangelog.com)
format:

```markdown
## [Unreleased]
### Added
### Changed
### Fixed

## [2.0.0-alpha.1] - 2026-08-xx
### Changed
- Build system restructured; vcpkg submodule removed (TI-007, TI-008)
```

Rules:

- Every PR that changes behaviour adds a line under `## [Unreleased]`, referencing its issue
  id.
- Entries are written for users, not for the compiler. "Accuracy no longer penalises corrected
  mistakes" — not "refactored InputAccuracyEngine".
- Release means moving the `Unreleased` block under a new version heading and tagging.
- **Breaking changes are listed first**, under an explicit `### Breaking` heading.

The 2.0.0 entry will need a prominent note that WPM and accuracy figures are not comparable to
1.0.0, because a user who has been tracking their numbers deserves to know why they moved.

---

## 9. Compatibility promises

**Within a MAJOR version:**

- Database migrations are forward-only and automatic. Opening a database from any earlier 2.x
  works.
- Config keys are never silently dropped. A renamed key is migrated with a backup written
  first; a removed key produces a warning for one MINOR before removal.
- CLI flags are never removed within a MAJOR. A deprecated flag keeps working and warns.
- Metric definitions never change within a MAJOR. Changing what "accuracy" means invalidates
  every historical row, and that is precisely what a MAJOR bump is for.
- Theme files keep working; new semantic colours get defaults.

**Across a MAJOR version:** all of the above may break, and the CHANGELOG says exactly how.

---

## 10. Release checklist

Applied at every tag, pre-release included:

1. `main` (or `v2`) is green on the full CI matrix.
2. Version bumped in `CMakeLists.txt`; `TYPEIT_VERSION_PRERELEASE` set or cleared.
3. `CHANGELOG.md` `Unreleased` block moved under the new heading and dated.
4. All issues in the milestone are closed, or explicitly moved to the next.
5. Full suite passes under ASan + UBSan.
6. `typeit --version` prints the expected string.
7. `typeit --doctor` is clean on a fresh machine.
8. Migrations verified from every prior schema version, and from an empty database.
9. Packages built and smoke-tested on clean Arch and clean Windows installs.
10. Annotated tag pushed; CI publishes the release artefacts.
11. Documentation checked against shipped behaviour — no document describes something that
    does not exist.

---

## 11. How to version properly — the practical guide

The policy above says what the numbers mean. This section is the procedure for picking one, and
the failure modes worth knowing about.

### 11.1 The decision procedure

Ask these in order and stop at the first "yes".

```
1. Does an existing user's data, config, or muscle memory stop working
   the way it did?                                              → MAJOR
     · a metric definition changed (their history is now incomparable)
     · a default keybinding changed
     · config keys removed, or their meaning changed
     · the database can no longer be opened by the previous version
     · a CLI flag removed or its meaning changed
     · data moved to a different directory

2. Can a user do something they could not do before, with everything
   they already do still working identically?                   → MINOR
     · a new mode, screen, provider, extractor, theme
     · a new config key with a default that preserves old behaviour
     · a new CLI flag
     · a new metric shown alongside the existing ones

3. Otherwise                                                    → PATCH
     · bug fixes, performance, refactoring, docs, build, CI
```

### 11.2 The question that decides it

For most changes the answer is obvious. When it is not, the useful question is:

> **If a user upgrades without reading anything, does something surprise them?**

A surprise is MAJOR. Not "does it crash" — surprise is a lower bar than breakage, and it is the
right bar, because a user whose 30-second personal best silently dropped by 8 WPM has been
harmed even though nothing crashed.

### 11.3 Worked examples from this project

| Change | Version | Why |
|---|---|---|
| Fix the accuracy defect so corrected mistakes stop compounding ([C4](CODEBASE_REVIEW.md#4-correctness-defects)) | **MAJOR** | It is a bug fix, and it still changes every recorded accuracy figure. Users would see their numbers jump with no explanation. This is the clearest case of "a fix that is nonetheless breaking" |
| Adopt the standard `(chars/5)/minutes` WPM formula | **MAJOR** | Same reasoning. Historical rows become incomparable |
| Add Race mode | MINOR | Purely additive |
| Add an EPUB extractor | MINOR | New capability, nothing existing changes |
| Add `text_section` (schema v2) | MINOR | Schema versions are independent; the migration is automatic and forward-only |
| Retire the `Ctrl+T` binding | **MAJOR** | Muscle memory is a user interface |
| Change the Race `ramp_up` default from 0.6 to 0.5 after tuning | MINOR | A tuning default, documented in the CHANGELOG. Arguably MAJOR if it invalidated race personal bests — decide by whether recorded values become incomparable, and if genuinely unsure, prefer MAJOR |
| Speed up history aggregation | PATCH | Invisible |
| Fix an out-of-bounds read | PATCH | Unless it changes observable output |
| Rename a config key | **MAJOR**, or MINOR with migration | If the old key keeps working with a deprecation warning, MINOR. If it stops working, MAJOR |

That last row is the general escape hatch: **a breaking change plus a compatibility shim is a
MINOR change.** Most MAJOR bumps are avoidable if you are willing to carry the shim, and the
choice is a cost decision, not a rule.

### 11.4 Pre-releases

Use them for anything you would be embarrassed to have someone install by accident.

- `-alpha.N` — incomplete; may not run. Alphas here mark internal phase boundaries.
- `-beta.N` — feature-complete for its scope, expect bugs. `2.0.0-beta.1` is the first tag
  worth daily use.
- `-rc.N` — believed shippable. Only fixes after this; a new feature resets to `beta`.

Increment the pre-release counter, never reuse it. `2.0.0-beta.2` follows `2.0.0-beta.1` even
if the change is trivial — tags are free and mutable tags are not.

### 11.5 Rules that prevent most versioning pain

1. **Never move a published tag.** If a release is wrong, publish the next patch. A moved tag
   breaks everyone who pinned it and is invisible when it happens.
2. **Never reuse a version number**, including pre-releases and including a release you deleted
   ten minutes later. Someone fetched it.
3. **Bump the version in its own commit**, touching only `CMakeLists.txt` and `CHANGELOG.md`.
   Mixing a bump into a feature commit makes the release history unreadable.
4. **Tag only from a commit CI has proven green.** The release workflow enforces this; do not
   route around it.
5. **Write the CHANGELOG entry when you write the code**, under `## [Unreleased]`. Nobody
   reconstructs six weeks of changes accurately at release time.
6. **One version per release artifact.** `typeit --version` must match the tag must match the
   package filename. The version guard checks all three.
7. **When genuinely unsure between MINOR and MAJOR, choose MAJOR.** An unnecessary MAJOR costs
   a version number. A missing one costs a user's trust in their own data.

### 11.6 When you get it wrong

| Situation | Do this |
|---|---|
| Released a MINOR that was actually breaking | Do **not** retag. Publish a PATCH that restores compatibility if possible; otherwise publish the MAJOR immediately, and document the mistake in both CHANGELOG entries |
| Tagged the wrong commit | Publish the next patch version from the right commit. Delete the GitHub *release* if it helps, but leave the tag |
| Forgot the CHANGELOG entry | Add it in the next release's PR and backfill the heading. The version guard now prevents this |
| Version in the source disagrees with the tag | The guard blocks the release before it publishes. Fix the source, tag again with the next number |

### 11.7 What the numbers are not

- **Not a marketing signal.** 2.0.0 here means "contracts broke", not "big release". A tiny
  breaking change is still MAJOR.
- **Not a progress bar.** 0.x is for things whose shape is still unknown. TypeIt has a shipped
  1.0, so it does not go back.
- **Not tied to the schema, config, or theme version.** Those are independent monotonic integers
  (§5). A schema bump does not imply an app bump, and vice versa. Coupling them is the most
  common versioning mistake in applications with a database.

---

## 12. Versioning in the pipeline

Mechanics live in [CI_CD §6](CI_CD.md#6-tier-1--versioning-guards-version-guardyml) and
[§14](CI_CD.md#14-version-bumping-version-bumpyml); this is what it means for the workflow.

### 12.1 What the pipeline enforces

Every one of these is a guard that fails a build, not a convention people remember:

| Guard | Prevents |
|---|---|
| Tag ↔ `project(VERSION)` must agree | Releasing `v2.1.0` from a tree that says `2.0.0` |
| Pre-release suffix consistency | Tagging `v2.0.0` with `alpha.1` still set in the cache variable |
| CHANGELOG has an entry for the tag | A release with no notes |
| No version literal outside `CMakeLists.txt` | Two versions disagreeing in one binary |
| Schema files changed ⇒ `user_version` bumped ⇒ migration test exists | Shipping a binary that cannot open its own database |
| Config key removed ⇒ `config_version` bumped ⇒ migration added | Silently discarding a user's settings |
| Behavioural PR ⇒ `Unreleased` entry (warning) | A CHANGELOG reconstructed from memory |

### 12.2 What it automates

- **Version derivation.** Tag builds take the version from the tag; `main` builds append
  `+<sha>`; branch builds append `-dev.<run>+<sha>`. Build metadata after `+` is ignored by
  SemVer precedence, so a development build can never outrank a release.
- **The bump itself.** `version-bump.yml` takes `major|minor|patch|prerelease`, edits
  `CMakeLists.txt`, moves the `Unreleased` block under a dated heading, and **opens a pull
  request**. It never pushes to `main`.
- **Release notes.** Extracted from the CHANGELOG section for the tag being released.
  Release Drafter accumulates merged PR titles continuously so the section is mostly written
  before you need it.
- **Publication.** Packages, checksums, SBOM, and provenance attestation, all gated on the
  guards and the full matrix passing first.

### 12.3 What it deliberately does not automate

**Choosing the number.** Conventional Commits could drive it — `feat:` → minor,
`BREAKING CHANGE:` → major — and for this project that is the wrong trade.

The accuracy fix in TI-037 is the proof. It is a bug fix. Any reasonable author writes it as
`fix(core): derive accuracy from the keystroke log`. Automation reads `fix:` and produces a
PATCH. But it changes every accuracy figure every user has ever recorded, which makes it MAJOR
under §11.1 rule 1 — and no regex over a commit subject was ever going to notice.

So the pipeline computes and suggests; a human confirms by merging the PR. That removes the
tedium and keeps the judgement, which is the correct split for a decision that is wrong exactly
when it matters most.
