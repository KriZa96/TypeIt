# Phase 9 — Compatibility, packaging, release

**Milestone:** `v2.0.0` · **Issues:** TI-135 – TI-145 · **Goal:** ship it.

No new features. This phase is about the difference between "works on my machine" and "works on
someone else's", which for a terminal application is most of the remaining risk.

---

## TI-135 — Terminal compatibility pass

**Type** test · **Size** L · **Priority** P0 · **Depends on** Phase 8 · **Docs** [UX §6.1](../UX.md#61-support-matrix)

Manual verification across the tier-1 matrix. Tedious, unautomatable, and the only way to find
out whether the capability detection written in Phase 4 actually matches reality.

**Matrix**

| Linux | Windows |
|---|---|
| alacritty, kitty, foot, GNOME Terminal, Konsole, xterm, WezTerm, tmux | Windows Terminal, WSL, PowerShell 7 in conhost |

**Per terminal, verify**
- [ ] Colours render correctly at the detected depth
- [ ] Detected depth and glyph set match what the terminal actually supports
- [ ] Box drawing and all glyphs render without gaps or overlap
- [ ] Wide characters (CJK, emoji) occupy the cells the app expects
- [ ] Every default keybinding is deliverable, or is reported as unavailable
- [ ] Resize works mid-session without state loss
- [ ] The typing caret lands where it should
- [ ] No flicker at the active frame rate

**Tier-2 spot checks:** Linux TTY (16 colours, no box drawing), st, urxvt, screen, VS Code
integrated terminal, cmd.exe, Git Bash/mintty, ConEmu.

**Acceptance**
- [ ] Results recorded per terminal in the issue, including failures and whether each was fixed
      or documented as a known limitation.
- [ ] Any detection mismatch found is fixed in `Capabilities::detect()` and gets a regression
      test.

---

## TI-136 — `--doctor` key tester

**Type** feat · **Size** S · **Priority** P2 · **Depends on** TI-076, TI-135 · **Docs** [UX §6.5](../UX.md#65-key-delivery)

Interactive mode showing what the terminal actually sends for each key press, so a user in an
unfamiliar terminal can find out why a binding does not work instead of guessing.

**Unit tests** (`KeyTesterTest.cpp`)
- Known synthetic events map to the expected names.
- An unrecognised sequence is displayed as raw bytes rather than swallowed.
- Exit works from within the tester (with a documented escape hatch if the exit key itself is
  undeliverable).

---

## TI-137 — Install rules

**Type** build · **Size** S · **Priority** P0 · **Depends on** TI-055 · **Docs** [BUILD §8](../BUILD.md#8-install-and-packaging)

**Tests**
- **Relocation test**, extended: install to a temp prefix, delete the source and build trees,
  run from the prefix — assets, themes, and the man page are all found.
- Install to a non-default prefix works.
- `--prefix` and `DESTDIR` (for packagers) both behave.
- Uninstall (manifest-based) removes what it installed and nothing else.

**Acceptance**
- [x] A binary installed from a deleted source tree runs — the final confirmation that
      [defect C2](../CODEBASE_REVIEW.md#4-correctness-defects) is gone for good. `cli.relocatable`
      installs, **moves** the tree, and runs from the new location with `TYPEIT_ASSETS_DIR`,
      `XDG_DATA_DIRS` and the working directory all pointed away — so the configured prefix and
      the `./assets` fallback are both unavailable and only the executable-relative lookup can
      succeed. Watched to fail by dropping the asset install rule.
- [x] The prefix contains **only** what this project installs. A dependency fetched into the
      build contributes its own `install()` rules, and the first install this project ever ran
      produced a prefix containing FTXUI's headers, static libraries, pkg-config file and CMake
      package config — and none of our own binary, because the rules for it did not exist yet.
      `FTXUI_ENABLE_INSTALL` is off now and the test walks the prefix.
- [ ] `--prefix` and `DESTDIR`, uninstall, and the man page: `DESTDIR` and the manifest-based
      uninstall are untested, and `docs/typeit.1` does not exist until TI-139.

---

## TI-138 — CPack packaging

**Type** build · **Size** M · **Priority** P1 · **Depends on** TI-137

**Scope**
- In: TGZ and DEB on Linux, ZIP on Windows; correct metadata, dependencies, and file layout.
- Out: an AUR PKGBUILD and a winget manifest — worthwhile, but post-2.0 and not blocking.

**Tests**
- Each package installs on a clean container/VM and the binary runs.
- The DEB passes `lintian` with no errors.
- Package contents match the install manifest exactly — no stray build artefacts.
- Version in the filename matches `--version`.

---

## TI-139 — Man page and `--help`

**Type** docs · **Size** S · **Priority** P2 · **Depends on** TI-074

**Tests**
- `--help` lists **every** flag in `CliOptions` — a table-driven test over the option registry,
  so a new flag without help text fails the build rather than shipping undocumented.
- The man page covers every flag, config key, and default.
- `man typeit` renders without warnings after install.

---

## TI-140 — Release CI

**Type** ci · **Size** M · **Priority** P1 · **Depends on** TI-138 · **Docs** [VERSIONING §10](../VERSIONING.md#10-release-checklist)

Tag push → build all packages → attach to a GitHub release with notes from the CHANGELOG.

**Tests**
- A dry run on a test tag produces the expected artefacts.
- Artefact names include the version and platform.
- A pre-release tag (`-beta.1`) is marked as a pre-release on GitHub.
- A failed build blocks publication rather than releasing a partial set.

---

## TI-141 — Fuzz targets

**Type** test · **Size** M · **Priority** P2 · **Depends on** Phase 6, TX-001 · **Docs** [TESTING §8](../TESTING.md#8-non-functional-checks)

libFuzzer targets on every place that consumes input the program did not produce:

| # | Target | Input origin |
|---|---|---|
| 1 | `TextBuffer::from_utf8` | arbitrary bytes from any imported file |
| 2 | The TOML config parser | a hand-edited user file |
| 3 | The keystroke-script parser | `--simulate` input |
| 4 | `MarkdownExtractor`, `SubtitleExtractor`, `CodeExtractor` | arbitrary imported files (TX-002 – TX-004) |
| 5 | `EpubExtractor` | a ZIP container from anywhere (TX-008) — bombs, traversal, malformed OPF |
| 6 | `HtmlExtractor` | **the open web** (TX-011) — the most hostile input in the project |

Targets 4–6 attach as their extractors land; 5 and 6 are the highest-value targets in the
project, because they are the only code that parses container and markup formats from sources
the user does not control.

**Acceptance**
- [ ] Each target runs 10 million iterations with no crash, hang, or leak.
- [ ] A seed corpus is committed per target.
- [ ] Any crash found becomes a unit test before it is fixed.
- [ ] Wired into [CI-015](PHASE-0A-cicd.md#ci-015--fuzzing-workflow): 60 s per target on PRs,
      30 min nightly with a cached corpus.

---

## TI-142 — Endless-mode soak test

**Type** test · **Size** S · **Priority** P2 · **Depends on** TI-120

**Acceptance**
- [ ] A simulated 2-hour endless run completes with memory growth **linear in keystrokes only**
      — nothing else grows.
- [ ] No file-descriptor or handle leaks.
- [ ] Frame time does not degrade over the run (the rolling-WPM index must not be silently
      rescanning).
- [ ] Clean under ASan and LeakSanitizer.

---

## TI-143 — README and screenshots

**Type** docs · **Size** M · **Priority** P1 · **Depends on** TI-135

**Scope**
- In: rewrite `README.md` for 2.0 — what it is, an asciinema recording, quick start, install per
  platform, a feature summary, links into `docs/`.
- Out: the design docs, which stay under `docs/`.

**Acceptance**
- [ ] Every documented command is copy-pasteable and verified on a clean machine.
- [ ] Recordings show timed mode, race mode, and the history screen.
- [ ] **The README states plainly that font size and family are terminal settings, not app
      settings** ([UX §1](../UX.md#1-what-is-and-is-not-customisable)) — better to answer that
      once in the README than repeatedly in issues.

---

## TI-144 — Documentation reconciliation

**Type** docs · **Size** M · **Priority** P0 · **Depends on** all phases

Every document in `docs/` was written before the code. Reconcile them with what actually
shipped.

**Scope**
- In: verify every claim in ARCHITECTURE, TECHNICAL, GAMEPLAY, UX, STYLE, BUILD, TESTING against
  the shipped binary; update constants, formulas, schema, and file layouts that drifted; mark
  any ADR that was revised, with the reason; retire `CODEBASE_REVIEW.md` to a historical note
  now that the code it reviews is deleted.
- Out: new documentation.

**Acceptance**
- [ ] No document describes something that does not exist.
- [ ] Every race constant in GAMEPLAY §3.5 matches the shipped presets after TI-129's tuning.
- [ ] The schema in TECHNICAL §5 matches the actual v1 schema exactly.
- [ ] Every config key in TECHNICAL §6 exists and every existing key is documented (a
      table-driven test over the config registry would make this permanent — do that).
- [ ] All internal links resolve.

---

## TI-145 — 2.0.0 release

**Type** chore · **Size** S · **Priority** P0 · **Depends on** TI-135 – TI-144 · **Docs** [VERSIONING §10](../VERSIONING.md#10-release-checklist)

Run the release checklist:

- [ ] `main` green on the full CI matrix
- [ ] Version bumped; `TYPEIT_VERSION_PRERELEASE` cleared
- [ ] `CHANGELOG.md` `Unreleased` block moved under `## [2.0.0]` and dated
- [ ] All milestone issues closed or explicitly moved to 2.1
- [ ] Full suite passes under ASan + UBSan
- [ ] `typeit --version` correct
- [ ] `typeit --doctor` clean on a fresh machine
- [ ] Migrations verified from empty and from every prior schema version
- [ ] Packages smoke-tested on clean Arch and clean Windows
- [ ] Annotated tag `v2.0.0` pushed; CI publishes artefacts
- [ ] Documentation matches shipped behaviour

**The CHANGELOG's 2.0.0 entry must lead with the breaking changes**, and must state prominently
that **WPM and accuracy figures are not comparable to 1.0.0**. A user who has been tracking
their numbers deserves to know why they moved, rather than concluding they got worse overnight.

---

## Phase exit criteria

- [ ] A released artefact installs and runs on a clean Arch box and a clean Windows box.
- [ ] It finds its assets, creates config and database on first run, and survives a mid-session
      resize.
- [ ] Tier-1 terminal matrix verified, with results recorded.
- [ ] Fuzzers clean; soak test clean.
- [ ] Documentation reconciled with reality.
- [ ] `v2.0.0` tagged and published.
