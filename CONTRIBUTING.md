# Contributing to TypeIt

The work is planned in [docs/issues/](docs/issues/README.md) — 171 issues, each with its own
tests and acceptance criteria. Pick one, or open an issue describing what you have in mind before
writing much code.

## Build and test

```bash
cmake --preset linux-gcc-debug
cmake --build --preset linux-gcc-debug
ctest --preset linux-gcc-debug
```

Before opening a pull request, also run the sanitizer preset — it is the one that catches what
the compiler cannot:

```bash
cmake --preset linux-clang-asan
cmake --build --preset linux-clang-asan
ctest --preset linux-clang-asan
```

`linux-gcc-debug` builds with warnings as errors, which is what CI does on every platform. Other
presets are listed in [docs/BUILD.md](docs/BUILD.md).

## Style

[docs/STYLE.md](docs/STYLE.md) is the full answer. The parts that come up most:

- `clang-format` decides layout; run it, do not argue with it. CI pins 18.1.8:
  `pip install clang-format==18.1.8 && git ls-files '*.cpp' '*.h' | xargs clang-format -i`
- No global mutable state, and nothing in `core` may include a UI header.
- Time is injected through `IClock`. A test that sleeps will be rejected.

## Commits and branches

Branches are `<type>/TI-###-slug`, matching the issue. Commit subjects follow
[Conventional Commits](https://www.conventionalcommits.org/) — `feat`, `fix`, `refactor`, `test`,
`docs`, `build`, `ci`, `perf`, `chore`, `style` — in the imperative mood, at most 72 characters,
with no trailing period. The body explains *why*; the diff already shows what.

One logical change per commit. A refactor and a behaviour change belong in separate commits.

The pull request title is linted the same way, because a squash merge makes the title the commit
subject.

## Definition of done

An issue is closed only when **every** line below is true. This is the contract; the per-issue
acceptance criteria are additional, not alternative.

1. The acceptance criteria in the issue are all ticked.
2. Unit tests exist for every behaviour the issue introduces, including the failure paths and the
   boundary cases.
3. A bug fix has a test that **fails before the fix and passes after**. Verify that ordering
   explicitly — a test written after the fix that has never been seen to fail proves nothing.
4. The full suite passes on the whole CI matrix: Linux gcc, Linux clang, Windows MSVC, plus ASan
   and UBSan.
5. Zero warnings with `TYPEIT_WERROR=ON` on all three compilers.
6. `clang-format` clean; no new `clang-tidy` findings.
7. No test sleeps. Time is injected through `IClock`.
8. No global mutable state introduced.
9. `CHANGELOG.md` updated under `## [Unreleased]` if behaviour changed, referencing the issue id.
10. Documentation in `docs/` updated if the issue changed anything a document describes.
11. Coverage thresholds for the layer still met ([docs/TESTING.md](docs/TESTING.md#9-coverage)).

The list is maintained in [docs/issues/README.md](docs/issues/README.md#definition-of-done); if
the two ever disagree, that one wins.

## What CI will check

Build and test on six configurations, ASan and UBSan, `clang-format`, `clang-tidy`, `cppcheck`,
spelling, markdown, links, shell scripts, workflow lint, and the pull request title. Design and
rationale: [docs/CI_CD.md](docs/CI_CD.md).
