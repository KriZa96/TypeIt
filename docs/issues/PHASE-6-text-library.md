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

**Acceptance**
- [x] Most of this was already true from TI-070; what was missing was everything about *bytes*.
- [x] A UTF-8 byte-order mark is **stripped**, not typed. It is valid UTF-8, which is exactly
      the problem: it decodes to U+FEFF and becomes a grapheme at the head of the text that the
      typist has to type and cannot see. Every file Notepad saves has one, so rejecting it
      would be rejecting most of Windows. It goes before the hash, so one article saved by two
      editors is one text.
- [x] A UTF-16 or UTF-32 file is rejected **by name**. "Invalid UTF-8 at byte 0" is true and
      useless: it sends somebody hunting for one bad character when the whole file is in
      another encoding, which is a different fix entirely. A genuinely corrupt character still
      reports its offset — naming encodings must not swallow the useful case, and there is a
      test that it does not.
- [x] The marks are spelled with explicit lengths. A `const char*` literal stops at its first
      NUL, so `"\x00\x00\xFE\xFF"` is the *empty* string and `starts_with` on it is true of
      everything: the first draft rejected every plain UTF-8 file as UTF-32BE, and the first
      test run said so.
- [x] The extension is never consulted. A typing test over source code is the point of the
      feature, and a whitelist is a list somebody's file is missing from.
- [x] A missing file, a directory and a permission failure are three different errors, because
      they are three different fixes. The permission case is armed on the port rather than
      arranged with `chmod`, which only fails for a user who is not root.

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
- [x] The terminal-reattachment case is explicitly tested; it is the non-obvious failure here.
      The process's standard input *is* the pipe, so once it has been drained there is no
      keyboard: FTXUI attaches to stdin, finds a pipe at end of file, and the program starts
      with a screen nobody can type into. `reattach_input` points `stdin` back at the device
      before the loop begins.
- [x] It is tested **without needing a terminal**, by reattaching to a file the test wrote and
      reading a byte back. A test that could only use the real one would pass or fail on how CI
      happens to run it, which is the same as not testing it. A device that is not there is a
      reported error rather than a crash — a service, a build step or a container without a tty
      is a normal thing to be.
- [x] Reading and reattaching are separate functions, so the reading half is testable without a
      pipe and the reattaching half without a terminal. One "read stdin and fix it up" would be
      testable with neither.
- [x] Binary comes through whole rather than partly consumed. Whether bytes are text is the
      importer's question and it already answers it with an offset; stopping at the first bad
      byte here would hand over a truncated file that looks fine.
- [x] An empty stream is empty, not an error: `rdbuf()` sets `failbit` when there was nothing to
      insert. `badbit` *is* reported — half an article silently imported as a whole one is the
      failure nobody notices.
- [x] Verified end to end as well: `printf 'piped text here' | typeit -` types that text, and
      `ctrl-q` still quits.

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

**Acceptance**
- [x] Already built and already tested — this issue is a checklist over work TI-070 did, and
      the honest answer is to say where each item lives rather than write a second file that
      asserts the same things in different words.
- [x] Identical content from different files is one entry:
      `TextLibraryServiceTest.TheSameContentTwiceIsOneText`, which also asserts that the second
      import reports `already_present` and hands back the first id.
- [x] Content differing only in normalised-away detail deduplicates:
      `NormalisationIsWhatDecidesWhetherTwoFilesAreTheSame`, plus TI-110's
      `ImportFileTest.ADosFileAndAUnixFileAreOneText` and `TheSameTextWithAndWithoutAMarkIsOneText`
      — the mark case is the one that was missing, and it is the reason a BOM is stripped
      *before* the hash rather than after.
- [x] Content differing meaningfully does not: `TheSameFileWithChangedContentIsANewText`.
- [x] The hash matches a reference implementation: `Sha256Test.TheFipsVectors` and
      `TheMillionAVector`, which are the published vectors rather than a second implementation
      to disagree with.

---

## TI-114 — Tagging and search

**Type** feat · **Size** S · **Priority** P2 · **Depends on** TI-059

**Unit tests** (`TagSearchTest.cpp`)
- Add, remove, and list tags; duplicates are idempotent.
- Search matches title and tags, case-insensitively, including non-ASCII.
- Multiple tag filters combine with AND (documented).
- No matches renders an empty state.

**Acceptance**
- [x] Search now looks at the **tags as well as the title**, which it did not. Somebody who
      tagged a text `rust` and called it something else types "rust" and expects to find it.
- [x] The search finds and the tag filter narrows: an OR across title and tags, ANDed with the
      tags asked for. Both halves have a test, and the combination has its own.
- [x] Every tag asked for, not any: `ListFiltersByEveryTagAskedFor`. Tagging twice is
      idempotent (`TaggingTwiceIsSomebodyClickingTwice`), and untagging removes only that one.
- [x] Case folding is **ASCII-only**, because `LIKE` is SQLite's: a search for `Č` will not
      match `č`, and fixing that needs ICU. An exact non-ASCII match does work and is asserted,
      so the limitation is a known shape rather than a surprise. Recorded here rather than
      quietly left for somebody to find.
- [x] No matches is an empty list, not an error. The empty *state* is the screen's job and
      belongs to TI-118.

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

**Acceptance**
- [x] The bookmark advances by **what was actually typed**, not by the chunk that was offered.
      A run abandoned half way through a chunk has read half a chunk, and advancing by the
      whole one would skip text nobody saw — the one failure that would make this feature worse
      than not having it.
- [x] Over-counting is clamped to the end rather than stored past it, so no reader of a
      bookmark has to defend against an offset pointing beyond the last grapheme.
- [x] The percentage is **exact** at both ends: 0.0 before starting and 1.0 at the end, not
      0.998. A book reported as 99.7% finished is a book somebody types one more chunk of to
      find nothing there.
- [x] A text nobody has started is at zero rather than an error — not having started is the
      normal state of most of a library, and a listing should not special-case it. A text
      nobody imported *is* an error: a confident zero there reads as "not started", and is not.
- [x] An empty text counts as finished rather than dividing by zero. There is nothing left to
      type either way, and 0/0 is not a percentage.
- [x] Re-importing changed content starts at zero, because deduplication is by content and
      changed content is a *different text* — so resuming is not merely avoided, it is
      impossible. The old bookmark stays with the version it was made against, which is
      asserted alongside. Re-importing the *same* text keeps its place, or re-adding a book
      would silently throw away a month of evenings.

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

**Acceptance**
- [x] A **shuffle bag**, not independent draws: every sentence is used once before any is used
      twice, and each pass is shuffled again rather than repeating one permutation. Sampling
      would show the same sentence three times in a row often enough to be noticed, and a
      typing test that repeats itself is one somebody stops reading and starts pattern-matching.
- [x] Abbreviations are a **list, not a rule**, because there is no rule: "Dr." ends in a stop
      and continues, "etc." ends in a stop and usually does not. Getting a rare one wrong costs
      a split in an odd place; getting `e.g.` wrong costs one in every technical article there
      is, which is most of what anybody imports.
- [x] A terminator only ends a sentence when whitespace or the end follows it, which is what
      keeps `3.14` and `example.com` whole. Closing quotes and brackets stay with the sentence
      they close, or the next one begins with a stray `"`.
- [x] Text with no terminator is one sentence — the usual shape of a code snippet, and the
      input this feature is most often given.
- [x] Nothing to shuffle is refused rather than served: an endless provider over no sentences
      is an endless stream of nothing, which hangs a run instead of reporting anything.
- [x] Fisher-Yates is written out rather than `std::shuffle`, for the same reason as
      `Prng::below` below — see TI-117.

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
- [x] Determinism from the seed holds across platforms — and it **did not**, which this issue
      is what uncovered. `Prng::below` used `std::uniform_int_distribution`, whose mapping from
      engine output onto a range is implementation-defined: libstdc++ and libc++ disagree, so a
      run recorded on Linux and replayed on Windows produced different text from the same seed.
      "Reproduce this run exactly" was untrue on precisely the machine somebody reads a bug
      report on. It is now rejection sampling written out — `std::mt19937_64` itself is
      specified exactly, so the engine was never the problem. `std::shuffle` has the same
      defect and is likewise replaced by a written-out Fisher-Yates.
- [x] A pinned expected chunk for a known seed, so the guarantee is a test rather than a hope.
      If it changes, a recorded replay has broken and somebody has to have meant it.
- [x] Sampling is **with replacement**, weighted by count. A shuffle bag would say "the" as
      many times as the source did and then not at all, which is the opposite of generating
      text in a source's style. The distribution is asserted over a thousand chunks against a
      deliberately loose bound: a tight one on a statistical property is a test that fails on a
      Tuesday.
- [x] Every word in the pool can come out. A weighted draw that never reaches the last entry is
      an off-by-one nobody would see in the ratios.
- [x] The frequency table is in **first-appearance order**, not sorted. A `std::map` would make
      the weighted pick depend on the alphabet rather than on the text — harmless right up
      until two runs are compared and the seeds no longer mean the same thing.
- [x] Punctuation and capitalisation are preserved when enabled and absent when disabled, and
      multi-byte characters survive either way: `ispunct` is only ever asked about ASCII bytes,
      so a UTF-8 continuation byte is never mistaken for punctuation.
- [x] An empty source, and a source whose every word is below the length threshold, are both
      refused before they can produce an endless stream of nothing.
- [x] Chunks never split a word: separators go between words and never at an edge, so two
      chunks joined cannot split a word or double a space.

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

**Acceptance**
- [x] `--import`, `--list-texts` and `--remove-text` are implemented and reach the library; the
      composition root answered all three with "not built yet" before this.
- [x] `--text` and `--text-id` are **different routes on purpose**, and the code says so:
      `--text` types a file once and imports nothing (GAMEPLAY §5.1), `--text-id` types
      something already in the library, which is what makes a bookmark worth keeping.
- [x] `--text-id` with an unknown id names the id and points at `--list-texts`, rather than
      starting a run over an empty string.
- [x] `--remove-text` **refuses without `--yes`** rather than prompting. A prompt on stdin is
      one a pipe cannot answer, and stdin here may well be a piped text — so the confirmation
      is a flag, and saying which flag is the whole message. `--yes` is new; the help-line count
      test had it pinned at 22 and now says 23.
- [x] Removal reports what else went: the tags and the bookmark, and that past sessions are
      kept and simply no longer name a text. "Removed" alone leaves somebody wondering about
      the runs they typed from it.
- [x] The listing is tab-separated with one header line, because `--list-texts | awk` is how
      somebody finds the id to pass to `--text-id`. Titles are flattened to one line first: a
      title can contain anything, and a listing piped into `cut` cannot.
- [x] A failure to read one text's progress is a dash, not the end of the listing — the other
      texts are still worth showing.
- [x] Exit codes are pinned by ctest cases over the real binary, which is the half a script
      reads and the half a unit test cannot see.

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
