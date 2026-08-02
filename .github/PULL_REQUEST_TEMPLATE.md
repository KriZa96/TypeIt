<!-- markdownlint-disable MD041 -- a pull request body opens with prose, not a heading -->
<!-- Title follows Conventional Commits and names the issue:
     feat(core): implement typing model state machine (TI-042) -->

Closes TI-000.

**What and why** — the diff shows what changed; say why it needed to.

## Definition of done

Full list in [CONTRIBUTING.md](../CONTRIBUTING.md#definition-of-done). Confirm each, or say why
it does not apply:

- [ ] Every acceptance criterion in the issue is ticked.
- [ ] Unit tests cover each new behaviour, including failure paths and boundaries.
- [ ] A bug fix has a test seen to fail before the fix and pass after.
- [ ] `ctest` passes on `linux-gcc-debug` and `linux-clang-asan` locally.
- [ ] No test sleeps; no new global mutable state.
- [ ] `CHANGELOG.md` updated under `## [Unreleased]` if behaviour changed.
- [ ] `docs/` updated if this changed anything a document describes.
