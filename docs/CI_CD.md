# TypeIt — CI/CD Design

*The complete GitHub Actions setup: what runs, when, why it earns its place, and what is
deliberately left out. This is the **first** thing built — see
[issues/PHASE-0A-cicd.md](issues/PHASE-0A-cicd.md).*

---

## 1. Why this comes first

The current repository has one workflow: build and test on `ubuntu-latest`, Debug, gcc-14.
That single job is why the codebase can contain three `-Wreorder` violations, a `-Wparentheses`
bug, undefined behaviour on an empty file, and Windows build instructions that have **never
been executed by a machine** — nothing was ever going to tell you.

Every later phase depends on being able to trust "it's green". So the pipeline is built before
the code it guards, on the codebase as it exists today. The first workflow lands against the
*current* vcpkg-submodule build and is updated in place as the build system improves. Building
CI last would mean writing a rebuild with no safety net and then bolting on a net that has
never caught anything.

**Guiding principle:** every job either prevents a class of defect that has already occurred in
this repository, or produces evidence that a documented claim is true. A job that does neither
is deleted.

---

## 2. Workflow map

```
.github/
├── workflows/
│   ├── ci.yml               push, PR      build + test matrix              (~6 min)
│   ├── quality.yml          push, PR      format, tidy, lint, typos, links (~3 min)
│   ├── sanitizers.yml       push, PR      ASan + UBSan                     (~5 min)
│   ├── security.yml         push, PR, wk  CodeQL, deps, Scorecard          (~8 min)
│   ├── coverage.yml         push, PR      gcovr + PR comment + gate        (~5 min)
│   ├── version-guard.yml    PR, tag       version/CHANGELOG/schema guards  (~1 min)
│   ├── benchmarks.yml       push(main)    perf tracking + regression alert (~4 min)
│   ├── fuzz.yml             PR / nightly  60 s per target / 30 min nightly
│   ├── docs.yml             push(main)    Pages deploy (docs + Doxygen)    (~3 min)
│   ├── nightly.yml          schedule      extended matrix, soak, floor     (~40 min)
│   ├── release.yml          tag v*        package, sign, attest, publish
│   └── version-bump.yml     dispatch      opens a version-bump PR
├── actions/
│   └── setup-build/         composite: toolchain + cache + configure
├── dependabot.yml
├── labeler.yml
├── CODEOWNERS
├── ISSUE_TEMPLATE/
└── pull_request_template.md
```

Shared setup lives in one composite action so a toolchain change is a one-line edit rather than
eleven.

---

## 3. Tier 1 — the core matrix (`ci.yml`)

| Job | Platform | Compiler | Config | Catches |
|---|---|---|---|---|
| `linux-gcc-debug` | ubuntu-latest | gcc | Debug | the baseline |
| `linux-gcc-release` | ubuntu-latest | gcc | RelWithDebInfo | optimiser-only bugs, UB that only manifests at `-O2` |
| `linux-clang-debug` | ubuntu-latest | clang | Debug | a second front end's warnings — clang and gcc disagree about plenty |
| `windows-msvc-debug` | windows-latest | MSVC | Debug | **the platform that has never once been compiled** |
| `windows-msvc-release` | windows-latest | MSVC | RelWithDebInfo | |
| `windows-clang-cl` | windows-latest | clang-cl | Debug | catches MSVC-specific workarounds that are actually bugs |

All with `TYPEIT_WERROR=ON`. Test results are emitted as JUnit XML
(`ctest --output-junit`) and surfaced in the PR check summary, so a failure names the test
rather than making you read a log.

**macOS** is offered as an optional tier-2 job. It is nearly free (FTXUI supports it), and it
catches libc++-versus-libstdc++ divergence that neither Linux nor Windows will. It is *not* a
supported platform and its failures do not block — the job exists for information, and the
distinction is stated in the workflow.

### Aggregator job

Path filters mean a docs-only PR skips the build matrix — but a skipped required check blocks a
PR forever. The fix is one `all-checks-passed` job that depends on every other job and passes
when they have either succeeded or been legitimately skipped. **That** is the single required
status check in branch protection.

---

## 4. Tier 1 — quality (`quality.yml`)

| Check | Tool | Why it earns its place here |
|---|---|---|
| Formatting | `clang-format --dry-run --Werror` | Ends formatting discussion permanently |
| Static analysis | `clang-tidy` (changed files on PR, full on `main`) | The `.clang-tidy` set catches the ownership and conversion problems that are endemic in the current code |
| Second opinion | `cppcheck` | Finds different things than clang-tidy; cheap |
| Workflow lint | `actionlint` | Lints the workflows themselves. Without it, a YAML typo is found by pushing a broken workflow to `main` |
| Shell lint | `shellcheck` | Any script in `scripts/` |
| Spelling | `typos` | Non-negotiable given the existing tree contains `Thrid`, `Dosent`, `behaivour`, `inputed`, and `neccessary` — in test names, comments, and documentation |
| Markdown | `markdownlint` | Keeps ~7,500 lines of docs consistent |
| Links | `lychee` | `docs/` is heavily cross-linked; a rename silently breaks a dozen references |
| Commit messages | `commitlint` (Conventional Commits) | Enforces [STYLE §11](STYLE.md#11-commits-and-branches). PR **title** is linted, since squash-merge makes the title the commit |
| CMake | `cmake-lint` | Optional; the build files are about to grow considerably |

Spelling and link checks are the ones people skip and then regret: they are seconds of CI time
and they are the only thing standing between 7,500 lines of documentation and slow rot.

---

## 5. Tier 1 — sanitizers (`sanitizers.yml`)

ASan + UBSan with `-fno-sanitize-recover=all`, on the full suite. Clang, Debug.

`-D_GLIBCXX_ASSERTIONS` (and `_LIBCPP_HARDENING_MODE=fast`) is enabled in the same job. That
turns out-of-range `operator[]` and iterator misuse into a clean abort, which is exactly the
class of bug the current `InputLineEngine` avoids only by accident.

TSan runs nightly rather than per-PR: there is one background thread posting events
([ADR-012](ARCHITECTURE.md#adr-012--one-frame-ticker-adaptive-rate-single-threaded-model)), so
data races are unlikely by design — but "unlikely by design" is a claim worth checking on a
schedule.

---

## 6. Tier 1 — versioning guards (`version-guard.yml`)

This is the workflow that makes the versioning policy real rather than aspirational.

| Guard | Fails when |
|---|---|
| **Tag ↔ source** | A tag `v2.1.0` is pushed while `CMakeLists.txt` says `2.0.0`. Refuses to release |
| **Pre-release consistency** | Tag `v2.0.0` with a non-empty `TYPEIT_VERSION_PRERELEASE`, or tag `v2.0.0-beta.1` whose suffix does not match |
| **CHANGELOG entry** | The tagged version has no `## [x.y.z]` heading in `CHANGELOG.md` |
| **Unreleased not empty** | A PR changes behaviour (touches `libs/`) but adds nothing under `## [Unreleased]` — a warning comment on the PR, not a hard failure |
| **No version literals** | A version string appears anywhere outside `CMakeLists.txt` ([VERSIONING §6](VERSIONING.md#6-single-source-of-truth)) |
| **Schema version guard** | Files under `libs/infra/schema/` changed without bumping `user_version` **and** adding a migration test from the previous version |
| **Config version guard** | A config key was renamed or removed without bumping `config_version` and adding a migration |
| **Metric-definition notice** | A PR touching `libs/core/metrics/` gets an automatic comment reminding that metric changes are MAJOR-only within a release line. Informational, never blocking |

The schema guard is the one worth highlighting: it is specific to this project, it prevents a
genuinely nasty failure (a released binary that silently cannot open its own database), and it
costs about twenty lines of shell.

### Version derivation

```
tag push  (refs/tags/v*)  →  VERSION = ${tag#v}          verified against source
main push                 →  VERSION = <project>+<sha7>  build metadata only
PR / branch               →  VERSION = <project>-dev.<run>+<sha7>
```

Build metadata after `+` is ignored by SemVer precedence, so a development build is never
mistaken for a release while still being traceable to a commit.

---

## 7. Tier 1 — coverage (`coverage.yml`)

gcovr on a gcc Debug build, with per-layer gates from
[TESTING §9](TESTING.md#9-coverage): `core` ≥ 90% (95% for `metrics` and `text`), `app` ≥ 85%,
`infra` ≥ 75%, `tui` ≥ 60%.

A PR comment shows the delta and, more usefully, **which new lines are uncovered** — a global
percentage barely moves when you add fifty untested lines to a large project, so the diff view
is what actually enforces the standard.

---

## 8. Tier 2 — security and supply chain (`security.yml`)

| Check | What it does |
|---|---|
| **CodeQL** | GitHub's semantic analysis for C++. Free on public repos, finds real memory and injection issues, results land in the Security tab |
| **MSVC Code Analysis** | `/analyze` results as SARIF — a genuinely different analyser from clang-tidy |
| **Dependency review** | Flags a dependency bump that introduces a known vulnerability, on the PR that does it |
| **OSV-Scanner** | Scans the pinned FTXUI / SQLite / toml++ / GoogleTest versions against the OSV database |
| **OpenSSF Scorecard** | Grades the repository's own supply-chain hygiene: pinned actions, branch protection, token permissions |
| **Dependabot** | Keeps **GitHub Actions** versions current. C++ dependencies are pinned deliberately and bumped by hand — automated C++ dependency bumps are noise |

### Hardening the workflows themselves

Workflows are code with credentials attached, and are treated accordingly:

- `permissions: contents: read` at the top level; elevated **per job**, never globally.
- Third-party actions pinned to a **full commit SHA**, not a tag. A tag is mutable; a
  compromised popular action with write access to a repository is a real and repeated incident
  class.
- `persist-credentials: false` on `actions/checkout` unless a job genuinely pushes.
- No `pull_request_target` combined with a checkout of untrusted code — the standard way
  repositories get compromised by a fork PR.
- Secrets referenced only in the jobs that need them; nothing that runs untrusted code sees a
  secret.
- `concurrency` groups cancel superseded runs on the same branch.
- Every job has a `timeout-minutes` so a hung runner cannot burn an hour.

---

## 9. Tier 2 — performance (`benchmarks.yml`)

Micro-benchmarks tracking the budgets in
[ARCHITECTURE §6.5](ARCHITECTURE.md#65-performance-budget):

- keystroke → updated view state
- rolling WPM over a 100k-event log (must stay O(window) — this catches an accidental rescan,
  which is exactly the kind of regression that is invisible until an endless run gets slow)
- full metrics computation
- history aggregation over 10,000 sessions
- text segmentation throughput

Results are pushed to `benchmark-action/github-action-benchmark`, which stores history on a
`gh-pages` branch, plots it, and **comments on a PR that regresses a benchmark by more than
20%**. Shared runners are noisy, so the alert threshold is deliberately loose — the value is in
the trend line, not in any single measurement.

Also tracked, because both are cheap and both silently rot: **binary size** and **cold build
time**.

---

## 10. Tier 2 — fuzzing (`fuzz.yml`)

libFuzzer targets on the three inputs that come from outside the program
([TESTING §8](TESTING.md#8-non-functional-checks)): `TextBuffer::from_utf8`, the TOML config
parser, and the keystroke-script parser. Once [text ingestion](TEXT_SOURCES.md) lands, the HTML
and EPUB extractors join them — those parse genuinely hostile input from the open web and are
the highest-risk code in the project.

- **On PR:** 60 seconds per target. Enough to catch an obvious new crash; short enough that
  nobody disables it.
- **Nightly:** 30 minutes per target, with the corpus cached between runs so coverage
  accumulates.
- A crash uploads the reproducer as an artifact, and **the reproducer becomes a unit test
  before the fix is written**.

---

## 11. Tier 2 — nightly (`nightly.yml`)

The extended matrix, run on a schedule because it is slow and rarely broken:

| Job | Catches |
|---|---|
| **Compiler-floor build** | gcc 13, clang 17, MSVC 19.38 exactly — verifies the documented minimum in [ARCHITECTURE §6.1](ARCHITECTURE.md#61-language-level-and-compiler-floor) is *real*. Documented floors are wrong more often than not |
| **aarch64 Linux** (QEMU) | Size and alignment assumptions; `char` signedness |
| **Alpine / musl** | glibc-specific assumptions |
| **`BUILD_SHARED_LIBS=ON`** | Missing visibility annotations |
| **TSan** | Confirms the single-threaded-model claim |
| **Valgrind** | Finds what ASan does not |
| **Long fuzz** | See above |
| **Soak test** | 2-hour endless run; asserts bounded memory (TI-142) |
| **Dependency freshness** | Reports when a pinned dependency has a newer release — reports, never bumps |

### PTY smoke test

A test that actually launches the built binary in a pseudo-terminal, waits for the first frame,
sends the quit key, and asserts a clean exit — run on Linux and Windows.

Snapshot tests cover rendering, but they never execute the real terminal initialisation path:
`ScreenInteractive` setup, Windows VT enabling and UTF-8 code pages
([TI-096](issues/PHASE-4-tui-cutover.md)), and terminal-mode restoration on exit. The PTY test
is the only thing that does. It also runs `--doctor` on every platform and archives the output,
which builds a real record of what each environment reports.

---

## 12. Tier 2 — documentation (`docs.yml`)

- Doxygen from the public headers → GitHub Pages.
- `docs/` rendered as a browsable site (mdBook) → same Pages site.
- Link check across the built site.
- Deploys only from `main`.

---

## 13. Release automation (`release.yml`)

Triggered by a `v*` tag. Every step is a gate:

```
1. version-guard          tag ↔ source ↔ CHANGELOG must agree, or stop
2. full CI matrix         all platforms, Release
3. full test suite        including sanitizers
4. package                CPack: TGZ + DEB (Linux), ZIP (Windows)
5. checksums              SHA256SUMS for every artifact
6. SBOM                   CycloneDX, attached to the release
7. provenance             actions/attest-build-provenance (SLSA build attestation)
8. release notes          extracted from the CHANGELOG section for this version
9. publish                GitHub release; pre-release flag set automatically for
                          -alpha / -beta / -rc tags
```

A failure at any step means **no partial release**. The most common release accident is
publishing three of four artifacts; making publication the last step after all builds succeed
prevents it structurally.

Provenance attestation and the SBOM are cheap now and awkward to retrofit. They let anyone
verify that a downloaded binary was built by this workflow from this commit.

**Package-manager publishing** — AUR, winget, Scoop, Homebrew — is deliberately post-2.0. Each
needs a stable release first, and each is its own maintenance commitment.

### Nightly pre-releases

The `v2` branch publishes a rolling `nightly` pre-release after a successful nightly run, so the
current state is installable without cutting a real version. Marked pre-release, never
advertised, always replaced.

---

## 14. Version bumping (`version-bump.yml`)

Manually dispatched, with an input of `major` / `minor` / `patch` / `prerelease`. It:

1. Reads the current version from `CMakeLists.txt`
2. Computes the next version
3. Updates `CMakeLists.txt` and `TYPEIT_VERSION_PRERELEASE`
4. Moves the `## [Unreleased]` block under the new heading, dated
5. Opens a **pull request** — it never pushes to `main`

### On fully automatic versioning

Conventional Commits could drive the version automatically (`feat:` → minor, `fix:` → patch,
`BREAKING CHANGE:` → major). For this project, **that is the wrong trade** and the reason is
worth stating rather than leaving as taste:

TypeIt's MAJOR bumps are triggered by things a commit prefix cannot see. Changing what
"accuracy" means invalidates every historical row a user has accumulated
([VERSIONING §9](VERSIONING.md#9-compatibility-promises)); that is a MAJOR bump, and it can
arrive in a commit that looks like a two-line `fix:`. Delegating that judgement to a regex over
commit subjects gets it wrong precisely when getting it right matters most.

So: commit messages are linted, the workflow *computes and suggests* the next version, and a
human confirms it by merging the PR. The automation removes the tedium, not the judgement.

---

## 15. Repository automation

| Feature | Verdict |
|---|---|
| **Dependabot** (Actions) | Yes — action versions rot and are a supply-chain surface |
| **Labeler** | Yes — auto-label PRs by changed path (`layer:core`, `docs`, `build`) |
| **Issue / PR templates** | Yes — the PR template carries the [definition of done](issues/README.md#definition-of-done) checklist |
| **CODEOWNERS** | Yes — trivial now, correct if the project ever gains a contributor |
| **Branch protection** | Yes — `main` and `v2` require the aggregator check and a linear history |
| **Release Drafter** | Yes — accumulates merged PR titles into a draft, so the CHANGELOG is never written from memory at release time |
| **Stale bot** | **No.** A solo project with a deliberate 145-issue backlog does not want a bot closing its own roadmap |
| **Merge queue** | **No.** Solves a problem that needs many concurrent contributors |
| **Auto-merge on green** | **No.** The point of review is review |
| **OSS-Fuzz** | Not yet. Revisit if the project gains users; the local fuzz jobs cover the same inputs |

---

## 16. Cost control

Free-tier minutes on public repositories are effectively unlimited, but slow CI is abandoned CI.

- **Caching:** `FETCHCONTENT_BASE_DIR` keyed on the dependency pin hashes; `ccache`/`sccache`
  keyed on compiler + flags. A warm cache should take the matrix from roughly 6 minutes to
  under 2.
- **Path filters:** a docs-only change runs `quality.yml` and `docs.yml`, not the build matrix.
- **Concurrency:** pushing twice cancels the first run.
- **Tiering:** expensive and rarely-failing checks (aarch64, musl, Valgrind, long fuzz, soak)
  run nightly, not per push.
- **Fail fast off** in the matrix: one platform failing should not hide whether the others are
  broken too. That information is worth the extra minutes.

**Target: under 5 minutes for the full PR check set on a warm cache.** Past roughly ten minutes
people stop waiting for CI and start merging on hope.

---

## 17. Rollout order

The pipeline is built incrementally so it is useful on day one rather than in week three. Each
step leaves CI green.

| Step | Issue | Delivers |
|---|---|---|
| 1 | CI-001 | Repo scaffolding, `actionlint`, hardened permissions, concurrency |
| 2 | CI-002 | Build + test the **current** code on Linux — an immediate safety net |
| 3 | CI-003 | The composite setup action and caching |
| 4 | CI-004 | Windows job — expected to fail; that failure is the first real portability data the project has ever produced |
| 5 | CI-005 | Full build matrix + the aggregator check |
| 6 | CI-006 | `quality.yml` |
| 7 | CI-007 | `sanitizers.yml` |
| 8 | CI-008 | `version-guard.yml` + version derivation |
| 9 | CI-009 | `coverage.yml` |
| 10 | CI-010 | `security.yml` + Dependabot |
| 11 | CI-011 | `release.yml` |
| 12 | CI-012 | `version-bump.yml` |
| 13 | CI-013 | `nightly.yml` |
| 14 | CI-014 | `benchmarks.yml` |
| 15 | CI-015 | `fuzz.yml` |
| 16 | CI-016 | `docs.yml` + Pages |
| 17 | CI-017 | PTY smoke test |
| 18 | CI-018 | Branch protection, templates, labeler, CODEOWNERS |

Steps 1–8 land before any rebuild code is written. Steps 9–18 land alongside phases 0–2, as the
things they measure come into existence — there is no point wiring a coverage gate before there
is a `core` library to cover.

---

## 18. What CI cannot do

Worth writing down so nobody mistakes a green tick for a guarantee:

- **It cannot verify the terminal experience.** Snapshot tests check bytes; the PTY test checks
  startup. Whether the app is *pleasant* in kitty at 100×30 is a human judgement, which is why
  [TI-135](issues/PHASE-9-release.md) is a manual pass across a written matrix.
- **It cannot tune the race ramp.** The properties in
  [TI-122](issues/PHASE-7-endless-race.md) are testable; whether Brutal feels brutal is not.
  Hence [TI-129](issues/PHASE-7-endless-race.md).
- **It cannot judge whether a change is breaking.** See §14.
- **It cannot cover what has no test.** Coverage gates measure the tests that exist, not the
  behaviours that matter. The per-issue test lists in the backlog are what makes coverage
  meaningful.
