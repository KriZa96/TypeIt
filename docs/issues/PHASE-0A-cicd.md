# Phase 0A — CI/CD pipeline

**Milestone:** `v2.0.0-alpha.0` · **Issues:** CI-001 – CI-018 · **Goal:** a pipeline that tells
the truth, built before the code it guards.

**This phase runs first — before TI-001.** Design and rationale in [CI_CD.md](../CI_CD.md).

CI-001 through CI-008 land against the **current** codebase, exactly as it is today, submodule
and all. That is deliberate: a safety net built after the rebuild has never caught anything.
CI-009 onward land alongside phases 0–2 as the things they measure come into existence.

Workflows are code with credentials attached. Every issue here follows the hardening rules in
[CI_CD §8](../CI_CD.md#hardening-the-workflows-themselves): least-privilege `permissions`,
third-party actions pinned to a full commit SHA, `persist-credentials: false`, and a
`timeout-minutes` on every job.

---

## CI-001 — Workflow scaffolding and hardening

**Type** ci · **Size** S · **Priority** P0 · **Depends on** — · **Docs** [CI_CD §2](../CI_CD.md#2-workflow-map), [§8](../CI_CD.md#hardening-the-workflows-themselves)

The foundation every other workflow inherits. Nothing is built yet; this makes the workflows
themselves reviewable and safe.

**Scope**
- In: `.github/workflows/` layout; top-level `permissions: contents: read`; `concurrency`
  groups that cancel superseded runs; `timeout-minutes` defaults; an `actionlint` job that lints
  the workflows on every change to `.github/`; a `scripts/pin-actions.sh` helper that resolves
  action tags to commit SHAs.
- Out: any build job.

**Tests**
- `actionlint` passes on the workflow files.
- A deliberately malformed workflow fails the lint job.
- A deliberately unpinned action (tag instead of SHA) fails a pin-check step.

**Acceptance**
- [ ] Every third-party action is pinned to a full 40-character SHA with the version in a
      trailing comment.
- [ ] No workflow grants `write` permission at the top level.
- [ ] Pushing twice to a branch cancels the first run.

---

## CI-002 — Build and test the current code on Linux

**Type** ci · **Size** S · **Priority** P0 · **Depends on** CI-001

The immediate safety net. Builds the repository **as it stands** — vcpkg submodule, gcc,
Debug — and runs the existing 41 tests.

**Scope**
- In: replace `cmake-single-platform.yml` with `ci.yml`; submodule checkout (removed later by
  TI-008); `ctest --output-on-failure --output-junit`; test results surfaced in the check
  summary.
- Out: matrix, caching, Windows.

**Acceptance**
- [ ] Green on the current `main` with no source changes.
- [ ] A deliberately failing test fails the job and the failing test is **named in the check
      summary**, not buried in a log.
- [ ] Runs in under 10 minutes cold.

---

## CI-003 — Composite setup action and caching

**Type** ci · **Size** M · **Priority** P0 · **Depends on** CI-002 · **Docs** [CI_CD §16](../CI_CD.md#16-cost-control)

`.github/actions/setup-build` — one composite action handling toolchain selection, CMake/Ninja
installation, dependency caching, and `ccache`/`sccache`. Every later job uses it, so a
toolchain change is a one-line edit rather than eleven.

**Scope**
- In: the composite action; `FETCHCONTENT_BASE_DIR` cache keyed on dependency pin hashes;
  compiler cache keyed on compiler and flags; cache-hit-rate reporting in the job summary.
- Out: —

**Acceptance**
- [ ] A warm cache measurably shortens the build; the numbers are recorded in the issue.
- [ ] Changing a dependency pin invalidates the cache; changing a source file does not.
- [ ] Cache size stays within GitHub's per-repository limit, with eviction understood.

---

## CI-004 — Windows job

**Type** ci · **Size** L · **Priority** P0 · **Depends on** CI-003 · **Docs** [REVIEW D8](../CODEBASE_REVIEW.md#5-build-tooling-and-ci)

The first time this codebase has ever been compiled by MSVC. The README has shipped Windows
build instructions since the beginning with **zero** automated verification that they work.

**Expect this job to fail on its first run.** That failure is the most valuable single output
of this phase — it is the project's first real data about its own Windows portability, and it
arrives while the codebase is 1,100 lines rather than 6,000.

**Scope**
- In: `windows-latest`, MSVC, Debug; `/utf-8`; fixing what it surfaces — expect strictness
  around `std::format`, narrowing conversions, `min`/`max` macro collisions, and path handling.
- Out: clang-cl and Release (CI-005).

**Tests** — the full existing suite must pass on Windows. A test that fails there is either a
real portability bug or a test that assumed Linux; both are in scope.

**Acceptance**
- [ ] Green MSVC Debug job.
- [ ] **Every fix required is written up in the PR body.** This is a portability record the
      project has never had; capture it while it is fresh.

---

## CI-005 — Full matrix and the aggregator check

**Type** ci · **Size** M · **Priority** P0 · **Depends on** CI-004 · **Docs** [CI_CD §3](../CI_CD.md#3-tier-1--the-core-matrix-ciyml)

Six configurations: Linux gcc Debug/Release, Linux clang Debug, Windows MSVC Debug/Release,
Windows clang-cl Debug. Plus optional non-blocking macOS.

**Scope**
- In: the matrix with `fail-fast: false`; path filters so docs-only changes skip it; an
  `all-checks-passed` aggregator job that is the single required status check.
- Out: —

**Acceptance**
- [ ] All six green.
- [ ] **A docs-only PR passes the aggregator with the build matrix skipped** — the failure mode
      where a skipped required check blocks a PR forever is explicitly tested.
- [ ] `fail-fast: false` — one platform failing does not hide the others.
- [ ] macOS, if enabled, is `continue-on-error` and documented as unsupported.

---

## CI-006 — Quality workflow

**Type** ci · **Size** M · **Priority** P0 · **Depends on** CI-005 · **Docs** [CI_CD §4](../CI_CD.md#4-tier-1--quality-qualityyml)

`quality.yml`: format, static analysis, and the lint checks that keep 7,500 lines of
documentation from rotting.

**Scope**
- In: `clang-format --dry-run --Werror`; `clang-tidy` (changed files on PR, full on `main`);
  `cppcheck`; `typos`; `markdownlint`; `lychee` link check; `shellcheck`; commitlint on the PR
  title.
- Out: CodeQL (CI-010).

**Tests**
- Each check fails on a deliberately introduced violation, verified per check.
- The `typos` check is expected to flag existing misspellings — `Thrid`, `Dosent`, `behaivour`,
  `inputed`, `neccessary` — in test names, comments, and docs. Fix them in this issue or add an
  explicit, dated allow-list entry; do not silence the tool wholesale.

**Acceptance**
- [ ] All checks green after the existing violations are fixed.
- [ ] `lychee` finds no broken link across `docs/` — there are hundreds of cross-references and
      they are only correct until the first rename.
- [ ] clang-tidy legacy exclusion is explicit and references TI-097, the issue that removes it.

---

## CI-007 — Sanitizer workflow

**Type** ci · **Size** S · **Priority** P0 · **Depends on** CI-005 · **Docs** [CI_CD §5](../CI_CD.md#5-tier-1--sanitizers-sanitizersyml)

**Scope**
- In: `sanitizers.yml` with ASan + UBSan, `-fno-sanitize-recover=all`, plus
  `-D_GLIBCXX_ASSERTIONS`.
- Out: TSan and Valgrind (nightly, CI-013).

**Acceptance**
- [ ] Full suite green under ASan + UBSan.
- [ ] A deliberately introduced leak and a deliberate out-of-bounds read both fail the job.
- [ ] Hardened-mode assertions are active — verified by an intentional `operator[]` overrun in a
      scratch build.

---

## CI-008 — Version guards and version derivation

**Type** ci · **Size** M · **Priority** P0 · **Depends on** CI-005, TI-002 · **Docs** [CI_CD §6](../CI_CD.md#6-tier-1--versioning-guards-version-guardyml), [VERSIONING §12](../VERSIONING.md#12-versioning-in-the-pipeline)

The workflow that makes the versioning policy real instead of aspirational.

**Scope**
- In: `version-guard.yml` implementing every guard in CI_CD §6; version derivation for tag,
  `main`, and branch builds.
- Out: the schema and config guards, which are added when there is a schema to guard — folded
  into TI-057 and TI-061 respectively, and listed here so they are not forgotten.

**Tests** — each guard gets a negative test proving it actually fails
- Tag `v9.9.9` against a source saying `2.0.0` → blocked.
- Tag `v2.0.0` with `TYPEIT_VERSION_PRERELEASE=alpha.1` still set → blocked.
- Tag with no matching `## [x.y.z]` CHANGELOG heading → blocked.
- A version literal introduced outside `CMakeLists.txt` → blocked.
- A `libs/` change with no `## [Unreleased]` entry → PR comment, not a failure.
- Derived version strings are correct for all three trigger types.

**Acceptance**
- [x] Every guard has a negative test. A guard nobody has watched fail is not a guard.
- [x] `typeit --version`, the tag, and the package filename can only ever agree.

---

## CI-009 — Coverage workflow

**Type** ci · **Size** M · **Priority** P1 · **Depends on** CI-005, TI-023 · **Docs** [CI_CD §7](../CI_CD.md#7-tier-1--coverage-coverageyml)

Lands with Phase 1, when there is a `core` library worth measuring.

**Scope**
- In: `coverage.yml` with gcovr; per-layer gates from
  [TESTING §9](../TESTING.md#9-coverage); a PR comment showing the delta **and which new lines
  are uncovered**.
- Out: —

**Acceptance**
- [ ] Gates enforced per layer; dropping below fails.
- [ ] The PR comment highlights uncovered *new* lines — a global percentage barely moves when
      you add fifty untested lines, so the diff view is what actually enforces the standard.

---

## CI-010 — Security and supply chain

**Type** ci · **Size** M · **Priority** P1 · **Depends on** CI-005 · **Docs** [CI_CD §8](../CI_CD.md#8-tier-2--security-and-supply-chain-securityyml)

**Scope**
- In: CodeQL for C++; `dependency-review-action` on PRs; OSV-Scanner against the pinned
  dependencies; OpenSSF Scorecard weekly; MSVC `/analyze` → SARIF; `dependabot.yml` for
  **GitHub Actions versions only**.
- Out: automated C++ dependency bumps — those are pinned deliberately and bumped by hand.

**Acceptance**
- [ ] CodeQL results appear in the Security tab.
- [ ] Dependabot opens PRs for outdated actions, and those PRs pass CI.
- [ ] Scorecard score recorded in the issue as a baseline.

> `security.yml` and `dependabot.yml` have landed. These three boxes are deliberately left
> open: each one is a statement about GitHub's side of the repository and can only be ticked
> after the first push, not from a local checkout.

---

## CI-011 — Release workflow

**Type** ci · **Size** L · **Priority** P1 · **Depends on** CI-008, TI-138 · **Docs** [CI_CD §13](../CI_CD.md#13-release-automation-releaseyml)

Tag-triggered, fully gated. Guards → matrix → tests → package → checksums → SBOM → provenance →
notes → publish.

**Scope**
- In: `release.yml` with all nine steps; pre-release flag set automatically for
  `-alpha`/`-beta`/`-rc` tags; CycloneDX SBOM; `actions/attest-build-provenance`; notes
  extracted from the CHANGELOG section for the tag.
- Out: package-manager publishing (AUR, winget, Scoop) — post-2.0.

**Tests**
- A dry run on a throwaway tag produces every expected artifact.
- A failure in any build step **blocks publication entirely** — explicitly tested, because the
  most common release accident is publishing three of four artifacts.
- A pre-release tag is marked pre-release on GitHub.
- Checksums verify; the SBOM parses; the attestation verifies with `gh attestation verify`.

**Acceptance**
- [ ] No partial release is possible.
- [ ] A downloaded artifact can be traced to the commit and workflow that built it.

---

## CI-012 — Version bump workflow

**Type** ci · **Size** S · **Priority** P2 · **Depends on** CI-008 · **Docs** [CI_CD §14](../CI_CD.md#14-version-bumping-version-bumpyml)

Manually dispatched with `major|minor|patch|prerelease`. Edits `CMakeLists.txt`, moves the
`## [Unreleased]` block under a dated heading, and **opens a pull request**.

**Tests**
- Each bump type produces the correct next version, including pre-release transitions
  (`2.0.0-beta.1` → `2.0.0-beta.2` → `2.0.0-rc.1` → `2.0.0`).
- The CHANGELOG block moves correctly, and an empty `Unreleased` block is refused.
- **It never pushes to `main`** — asserted, not assumed.

**Acceptance**
- [x] The workflow suggests; a human confirms by merging. It does not choose the number on its
      own, for the reason in [VERSIONING §12.3](../VERSIONING.md#123-what-it-deliberately-does-not-automate).

---

## CI-013 — Nightly workflow

**Type** ci · **Size** L · **Priority** P2 · **Depends on** CI-005 · **Docs** [CI_CD §11](../CI_CD.md#11-tier-2--nightly-nightlyyml)

The slow, rarely-broken checks.

**Scope**
- In: compiler-floor build (gcc 13, clang 17, MSVC 19.38 exactly); aarch64 via QEMU;
  Alpine/musl; `BUILD_SHARED_LIBS=ON`; TSan; Valgrind; dependency-freshness report; rolling
  `nightly` pre-release from `v2`.
- Out: long fuzz (CI-015) and the soak test (TI-142), which attach to this workflow when they
  exist.

**Acceptance**
- [x] **The compiler floor is verified to be real**, not aspirational — documented minimums are
      wrong more often than not, and this is the only thing that would ever tell us.
- [x] A nightly failure opens or updates an issue automatically rather than passing unnoticed.

> The rolling `nightly` pre-release is the one piece deferred: it needs artifacts to attach,
> so it lands with CPack (TI-138) and `release.yml` (CI-011).

---

## CI-014 — Benchmarks and regression tracking

**Type** ci · **Size** M · **Priority** P2 · **Depends on** TI-039 · **Docs** [CI_CD §9](../CI_CD.md#9-tier-2--performance-benchmarksyml)

**Scope**
- In: benchmarks for the budgets in
  [ARCHITECTURE §6.5](../ARCHITECTURE.md#65-performance-budget); history stored on `gh-pages`;
  a PR comment on a >20% regression; binary size and cold build time tracked alongside.
- Out: —

**Acceptance**
- [ ] The rolling-WPM benchmark would catch an accidental full-log rescan — the specific
      regression that is invisible until an endless run gets slow.
- [ ] The alert threshold is loose enough that shared-runner noise does not cry wolf; the value
      is the trend line.

---

## CI-015 — Fuzzing workflow

**Type** ci · **Size** M · **Priority** P2 · **Depends on** TI-141 · **Docs** [CI_CD §10](../CI_CD.md#10-tier-2--fuzzing-fuzzyml)

**Scope**
- In: 60 s per target on PRs, 30 min nightly with a cached corpus; crash reproducers uploaded
  as artifacts.
- Out: OSS-Fuzz.

**Acceptance**
- [ ] PR fuzzing is short enough that nobody is tempted to disable it.
- [ ] **A crash reproducer becomes a unit test before the fix is written** — stated in the
      workflow's failure message so the rule travels with the failure.
- [ ] Targets extend to the HTML and EPUB extractors once
      [TEXT_SOURCES](../TEXT_SOURCES.md) lands — those parse genuinely hostile input.

---

## CI-016 — Documentation site

**Type** ci · **Size** S · **Priority** P3 · **Depends on** CI-006 · **Docs** [CI_CD §12](../CI_CD.md#12-tier-2--documentation-docsyml)

**Scope**
- In: Doxygen from the public headers; `docs/` rendered with mdBook; both deployed to GitHub
  Pages from `main`; link check across the built site.
- Out: —

**Acceptance**
- [x] Pages deploys only from `main`.
- [ ] Doxygen emits no warnings — an undocumented public class is a warning
      ([STYLE §9](../STYLE.md#9-comments-and-documentation)).

> `WARN_AS_ERROR = FAIL_ON_WARNINGS` is on, but `WARN_IF_UNDOCUMENTED` stays off until the
> legacy tree under `include/` is deleted at the cutover (TI-097). Turning it on against code
> that predates the rule and is being deleted rather than documented would only teach everyone
> to ignore the job. Flip it in TI-097 and tick this then.

---

## CI-017 — PTY smoke test

**Type** ci · **Size** M · **Priority** P1 · **Depends on** TI-092, CI-005 · **Docs** [CI_CD §11](../CI_CD.md#pty-smoke-test)

Launch the real binary in a pseudo-terminal, wait for the first frame, send quit, assert a clean
exit — on Linux and Windows.

Snapshot tests check bytes; they never execute terminal initialisation. This is the only thing
that exercises `ScreenInteractive` setup, Windows VT enabling and UTF-8 code pages (TI-096), and
terminal-mode restoration on exit.

**Scope**
- In: the harness; runs at 80×24 and 120×40, and with `TERM=dumb`; `--doctor` run on every
  platform with output archived.
- Out: visual verification, which is inherently manual (TI-135).

**Tests**
- Clean startup and exit on both platforms.
- Terminal mode and code page are restored after exit — asserted by querying them afterwards.
- `TERM=dumb` degrades rather than crashing.
- A non-zero exit or a hang fails the job.

**Acceptance**
- [ ] The archived `--doctor` output builds a real record of what each environment reports.

---

## CI-018 — Repository automation and protection

**Type** ci · **Size** S · **Priority** P2 · **Depends on** CI-005 · **Docs** [CI_CD §15](../CI_CD.md#15-repository-automation)

**Scope**
- In: branch protection on `main` and `v2` requiring the aggregator check and linear history;
  `labeler.yml` auto-labelling by changed path; issue and PR templates carrying the
  [definition of done](README.md#definition-of-done); `CODEOWNERS`; Release Drafter.
- Out: stale bot, merge queue, auto-merge — all rejected in CI_CD §15 with reasons.

**Acceptance**
- [ ] `main` cannot be pushed to directly.
- [ ] A PR is auto-labelled by the layer it touches.
- [ ] The PR template checklist matches the definition of done, so it cannot drift.

---

## Phase exit criteria

Phase 0A is complete when CI-001 – CI-008 are done; CI-009 – CI-018 close during phases 0–2 as
their subjects come into existence.

- [ ] The full PR check set completes in **under 5 minutes on a warm cache**. Past ten minutes
      people stop waiting for CI and start merging on hope.
- [ ] Every guard and every quality check has been **watched to fail** on a deliberate
      violation. An untested gate is decoration.
- [ ] Windows is built and tested on every push, for the first time in this project's history.
- [ ] No workflow holds more permission than it needs; every third-party action is SHA-pinned.
- [ ] A docs-only PR passes without running the build matrix, and without deadlocking on a
      skipped required check.
