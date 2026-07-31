# Phase 6 — Text library

**Milestone:** `v2.0.0-beta.1` · **Issues:** TI-110 – TI-119 · **Goal:** the headline
"type anything you want" feature, complete.

At the end of this phase TypeIt is worth using daily. The infrastructure has existed since
Phase 2; this phase builds the ingestion routes, the providers that make arbitrary text work in
every mode, and the UI to manage it.

---

## TI-110 — Import from file

**Type** feat · **Size** S · **Priority** P0 · **Depends on** TI-070 · **Docs** [GAMEPLAY §5.1](../GAMEPLAY.md#51-getting-text-in)

**Unit tests** (`ImportFileTest.cpp`)
- Plain UTF-8, UTF-8 with BOM (stripped), and CRLF files all import correctly.
- A UTF-16 file is detected and rejected with a message naming the encoding, rather than
  producing garbage.
- Invalid UTF-8 is rejected with the byte offset.
- An empty or whitespace-only file is rejected with a clear message — the case that currently
  triggers [defect C1](../CODEBASE_REVIEW.md#4-correctness-defects).
- A very large file (10 MB) is rejected with the limit named, or chunked, per the documented
  rule.
- A missing file, a directory, and a permission-denied file each produce distinct errors.
- A file with no extension, and one with a `.md`/`.rs`/`.txt` extension, all import.

---

## TI-111 — Import from stdin

**Type** feat · **Size** S · **Priority** P1 · **Depends on** TI-110

`cat article.txt | typeit -`

**Unit tests** (`ImportStdinTest.cpp`)
- Piped input imports correctly.
- Empty stdin is rejected.
- Binary data on stdin is rejected as invalid UTF-8, not partially consumed.
- A large stream is read completely without truncation.
- With stdin used for text, the TUI still attaches to the terminal correctly (`/dev/tty` on
  Linux, `CONIN$` on Windows) — otherwise the app starts with no keyboard.

**Acceptance**
- [ ] The terminal-reattachment case is explicitly tested; it is the non-obvious failure here.

---

## TI-112 — Import by paste

**Type** feat · **Size** M · **Priority** P2 · **Depends on** TI-110

A multi-line input area in the library screen.

**Unit tests** (`PasteImportTest.cpp`)
- Multi-line input is captured with line breaks intact.
- A bracketed-paste sequence is handled as one event, not as a keystroke storm.
- Very large pastes are accepted or rejected against the documented limit.
- Non-ASCII paste content survives intact.
- Cancelling discards without storing.

---

## TI-113 — Content deduplication

**Type** feat · **Size** S · **Priority** P1 · **Depends on** TI-110

SHA-256 over normalised content.

**Unit tests** (`ContentHashTest.cpp`)
- Identical content from different files yields one library entry.
- Content differing only in normalised-away detail (CRLF vs LF, smart quotes) deduplicates —
  which is the point of hashing the *normalised* form.
- Content differing meaningfully does not.
- Re-import reports "already in library" and returns the existing id.
- The hash matches a reference SHA-256 implementation for known inputs.

---

## TI-114 — Tagging and search

**Type** feat · **Size** S · **Priority** P2 · **Depends on** TI-059

**Unit tests** (`TagSearchTest.cpp`)
- Add, remove, and list tags; duplicates are idempotent.
- Search matches title and tags, case-insensitively, including non-ASCII.
- Multiple tag filters combine with AND (documented).
- No matches renders an empty state.

---

## TI-115 — Bookmarks and chunk progress

**Type** feat · **Size** M · **Priority** P1 · **Depends on** TI-049, TI-059 · **Docs** [GAMEPLAY §2.3](../GAMEPLAY.md#23-quote)

Type a book across many sessions and resume where you stopped.

**Unit tests** (`BookmarkTest.cpp`)
- Completing a chunk advances the bookmark by exactly that chunk.
- An abandoned session advances the bookmark only by what was actually completed.
- Reaching the end marks 100% and offers a reset.
- Resetting returns to offset 0.
- Progress percentage is exact at 0%, mid, and 100%.
- Re-importing a changed text resets the bookmark rather than resuming at a now-meaningless
  offset — silently resuming into shifted content would be worse than starting over.

---

## TI-116 — `ShuffledSentenceProvider`

**Type** feat · **Size** M · **Priority** P1 · **Depends on** TI-048 · **Docs** [GAMEPLAY §5.4](../GAMEPLAY.md#54-providers)

**Unit tests** (`ShuffledSentenceProviderTest.cpp`)
- Sentence splitting handles `.`, `!`, `?`, quotes after terminators, and common abbreviations
  ("Dr.", "e.g.") without splitting mid-sentence.
- Text with no sentence terminator yields one sentence.
- The same seed produces the same order; different seeds differ.
- The stream never ends.
- No sentence repeats until the pool is exhausted (documented shuffle-bag behaviour).

---

## TI-117 — `WordPoolProvider`

**Type** feat · **Size** M · **Priority** P0 · **Depends on** TI-048 · **Docs** [TECHNICAL §8.5](../TECHNICAL.md#85-word-pool-generation)

The provider that makes "any text" compose with "infinite mode": extract a text's vocabulary
with frequency weights and generate an endless stream in that text's style. Import a Rust
manual, race against endless Rust-manual vocabulary.

**Unit tests** (`WordPoolProviderTest.cpp`)
- The frequency table matches a hand-counted small input.
- Sampling is weighted — over 10,000 samples, the observed distribution matches the source
  within a documented tolerance (chi-squared or a simple ratio bound).
- The same seed reproduces the identical stream, exactly.
- Punctuation and capitalisation ratios are preserved when enabled, absent when disabled.
- A single-word source produces a valid (if dull) stream.
- An empty source is rejected before it can produce an empty infinite stream.
- Generated chunks never split a word.

**Acceptance**
- [ ] Determinism from the seed holds across platforms — a race replay must be reproducible.

---

## TI-118 — `TextLibraryScreen`

**Type** feat · **Size** M · **Priority** P1 · **Depends on** TI-110 – TI-115 · **Docs** [UX §3.6](../UX.md#36-text-library)

**Unit tests** (`TextLibraryScreenTest.cpp`)
- Listing shows title, word count, difficulty, and progress.
- Import, paste, tag, and delete flows work end to end.
- **Deleting confirms** and reports what will happen to sessions referencing the text.
- Search filters live as you type.
- An empty library renders a helpful first-run state.
- Long titles truncate without breaking the layout, including with wide characters.
- Snapshot at 80×24.

---

## TI-119 — Text CLI flags

**Type** feat · **Size** S · **Priority** P2 · **Depends on** TI-074, TI-110

`--text`, `--text-id`, `--import`, `--list-texts`, `--remove-text`, and `-`.

**Unit tests** (`TextCliTest.cpp`)
- Each flag performs its action and exits with the right status code.
- `--text` is one-shot and does **not** import into the library (documented distinction).
- `--text-id` with an unknown id errors clearly.
- `--remove-text` confirms unless `--yes` is given.
- `--list-texts` output is stable and machine-parseable.

---

## Phase exit criteria

- [ ] A 500 KB UTF-8 document imports in under a second, is typed across multiple sessions with
      the bookmark advancing correctly, and generates a coherent endless word pool.
- [ ] Invalid UTF-8 is rejected with a byte offset, never a crash and never mojibake.
- [ ] Deduplication works on normalised content.
- [ ] Provider output is reproducible from its seed, across platforms.
- [ ] Every ingestion route — file, stdin, paste, builtin — is covered by tests.
- [ ] **This is the first tag worth using daily.** Use it for a week before starting Phase 7.

> `v2.0.0-beta.1` also contains [Phase 6A](PHASE-6A-text-sources.md) waves 1–2 — the
> fetch/extract split, Markdown/code/subtitle extractors, sections, schema v2, and the
> typing-readiness pass. Waves 3–4 (EPUB, converters, web) ship in `2.1.0`.
