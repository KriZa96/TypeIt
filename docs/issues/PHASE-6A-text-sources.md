# Phase 6A — Extended text sources

**Milestone:** `v2.0.0-beta.1` (waves 1–2) and `v2.1.0` (waves 3–4) · **Issues:** TX-001 – TX-012

**Goal:** type from anything — Markdown, source code, subtitles, ebooks, web pages, or whatever
a converter on the user's machine can handle.

Design and rationale: [TEXT_SOURCES.md](../TEXT_SOURCES.md). Architectural decisions:
[ADR-013, ADR-014, ADR-015](../ARCHITECTURE.md#adr-013--ingestion-is-a-three-stage-pipeline-separate-from-text-supply).

Phase 6 (TI-110 – TI-119) delivers plain-text import. This phase generalises it into a pipeline
and adds formats. Each wave is independently useful, so the work can stop after any of them
without leaving something half-built.

| Wave | Issues | Content |
|---|---|---|
| 1 | TX-001 – TX-004 | The pipeline, plus Markdown, code, and subtitle extractors |
| 2 | TX-005 – TX-007 | Sections, schema v2, typing-readiness pass |
| 3 | TX-008 – TX-009 | EPUB, external converter hook |
| 4 | TX-010 – TX-012 | HTTP fetching, HTML readability |

---

## TX-001 — Split ingestion into fetch and extract

**Type** refactor · **Size** M · **Priority** P0 · **Depends on** TI-110, TI-118 · **Docs** [TEXT_SOURCES §2–4](../TEXT_SOURCES.md#2-the-pipeline), [ADR-013](../ARCHITECTURE.md#adr-013--ingestion-is-a-three-stage-pipeline-separate-from-text-supply)

Refactor the Phase 6 import path into `IContentFetcher` → `ITextExtractor` → `TextNormalizer`,
with an `ExtractorRegistry` resolving by MIME type. Behaviour is unchanged; the seams are new.

**Scope**
- In: the two interfaces; `FetchedContent` and `ExtractedText`; the registry; `FileFetcher`,
  `StdinFetcher`, `PasteFetcher`, `PlainTextExtractor` reimplemented behind them; MIME detection
  by magic bytes, then extension, then user override.
- Out: any new format.

**Unit tests** (`ExtractorRegistryTest.cpp`, `MimeDetectionTest.cpp`, `FetcherTest.cpp`)
- Registry resolves each registered MIME type to the right extractor; an unknown type falls
  through to the external hook, and then to a clear error.
- Two extractors claiming the same MIME type is a startup error, not a silent last-wins.
- Magic-byte detection wins over a misleading extension (a `.txt` file that is actually a ZIP).
- Detection on a zero-byte file, a 1-byte file, and a file of pure NUL bytes.
- User override in config beats detection.
- Every Phase 6 import test still passes unchanged — this is a refactor, and the existing tests
  are the proof.

**Acceptance**
- [x] No behaviour change; the Phase 6 suite is green **without edits** — and it earned its
      keep. The first cut of the registry let a `.md` file resolve to `text/markdown`, which no
      extractor claimed, so a file that imported yesterday was refused. `PlainTextExtractor`
      claims markdown, code and subtitles until TX-002 and TX-004 take them, and the registry's
      no-two-claims rule will say so loudly if either forgets.
- [x] Adding a format now requires one new file plus one registration.
- [x] **Two extractors claiming one type is a startup error, not last-wins.** A silent
      overwrite lets the build order decide which one runs, and the symptom is an EPUB
      extracted as a ZIP on one machine and correctly on another — a bug nobody reproduces. A
      rejected registration claims *none* of its types, because half-applied is a registry
      nobody can reason about.
- [x] Magic bytes beat a misleading extension. A `.txt` that is really a ZIP is a download
      named badly or an EPUB somebody renamed; believing the name hands a ZIP to the plain-text
      extractor, which reports invalid UTF-8 and sends the user hunting for a corrupt character
      in a file that is not text at all. A user override beats both — they can see the file and
      this cannot.
- [x] Detection survives a zero-byte file, a one-byte file and a file of pure NULs: each is
      shorter than most signatures, and a comparison reading past the end would be undefined
      rather than merely wrong.
- [x] Only formats this project can name are detected. A general-purpose sniffer would be a
      second `file(1)` to maintain, and every type it knew that no extractor handled would be a
      more confident way of failing.
- [x] No signature and no extension is `text/plain`, because that is usually somebody's notes
      and refusing it would refuse the commonest thing anybody imports.
- [x] A charset parameter is matched on the type alone: an extractor handling `text/plain` but
      not `text/plain;charset=utf-8` would fail on exactly the files somebody bothered to label.
- [x] The service registers the built-in extractors itself. A service that could be handed an
      empty registry would be one that fails to import a plain text file, which is not a state
      worth being able to construct.

---

## TX-002 — Markdown extractor

**Type** feat · **Size** S · **Priority** P1 · **Depends on** TX-001

**Scope**
- In: strip or preserve markup (configurable, default strip); headings become section
  boundaries; code fences preserved verbatim; link text kept and URLs dropped.
- Out: full CommonMark compliance — this is extraction, not rendering.

**Unit tests** (`MarkdownExtractorTest.cpp`) — table-driven
- Headings, emphasis, links, images, inline code, lists, block quotes, and tables each extract
  as documented.
- A fenced code block keeps its whitespace exactly.
- Reference-style links resolve; an unresolved reference degrades to its text.
- Front matter (YAML/TOML) is stripped.
- HTML embedded in Markdown is stripped in strip mode.
- `preserve_markup = true` returns the source unchanged.

**Acceptance**
- [x] Every case above passes, and the question each one asks is the same: *did somebody write
      this, or did a renderer need it?* Link text was written and the URL beside it was not. A
      heading's words were written and its hashes were not.
- [x] **A fenced block is copied byte for byte.** It is the one part of a Markdown file that is
      already what somebody wants to type, and every space in it is load-bearing — stripping
      markup inside it would produce code that does not compile.
- [x] Markup inside an inline code span is code, not markup. The asterisks in `` `a * b` `` are
      multiplication, and this is the commonest way a stripper corrupts a technical document.
- [x] An image goes entirely rather than leaving its alt text behind: alt text describes a
      picture to somebody who cannot see it, and typing "photograph of a lighthouse"
      mid-paragraph is a non sequitur.
- [x] An unresolved reference degrades to its text — a typo in a label costs the reader a link,
      not a sentence — while brackets that define nothing stay brackets, because `[sic]` and
      `[1]` are things people write in prose.
- [x] `a < b` survives. Treating every `<` as a tag would eat the rest of the line whenever a
      document does arithmetic.
- [x] Unterminated front matter is a thematic break rather than a document that is entirely
      metadata; otherwise a file opening with a horizontal rule is swallowed whole.
- [x] Sections cover the text without gaps, including the prose before the first heading: a
      bookmark measured in offsets has to land inside a section wherever it lands.
- [x] Every unbalanced construct — `[unclosed`, `` `unclosed ``, `<unclosed`, a trailing
      backslash — is scanned without reading past the end, which would be undefined rather than
      merely wrong.
- [x] `text/markdown` moved off `PlainTextExtractor`. The registry's no-two-claims rule is what
      makes that a compile-and-run failure rather than a silent race between two extractors.

---

## TX-003 — Code extractor

**Type** feat · **Size** M · **Priority** P1 · **Depends on** TX-001 · **Docs** [TEXT_SOURCES §4](../TEXT_SOURCES.md#why-subtitles-and-code-are-on-that-list)

Typing code is what a large share of this application's likely users do all day, and it
exercises symbols, brackets, and indentation that prose never touches.

**Scope**
- In: whitespace and indentation preserved **exactly**; tabs kept as tabs when the file uses
  them; language detected by extension; top-level definitions become sections; optional
  comment stripping; a warning when a file mixes tabs and spaces, since that makes a file
  effectively untypeable.
- Out: parsing. Section detection is a brace/indent heuristic, and says so.

**Unit tests** (`CodeExtractorTest.cpp`)
- Indentation is byte-identical to the source for tab-indented, space-indented, and mixed files.
- Trailing whitespace is preserved or stripped per config, and the default is documented.
- Very long lines (minified files) are detected and reported as poor typing material.
- Section detection on C++, Python, and Rust fixtures.
- Non-UTF-8 source files are rejected with a byte offset.
- A file with CRLF line endings normalises without corrupting indentation.

**Acceptance**
- [x] Code mode interacts correctly with `strict_spaces`, which matters far more here than in
      prose. With the indentation preserved, an indented line is four keystrokes before any
      letter arrives, and the setting decides whether skipping them is an error — in prose a
      missing space is a typo, and in Python it is a different program.
- [x] **Indentation is byte-identical to the source**, for tab-indented, space-indented and
      mixed files alike. This forced a change one level up: the library's normalisation
      collapses runs of whitespace and expands tabs, which between them turn every indented
      line into one leading space, so `ExtractedText` gained a normalisation the extractor
      chooses. Trusting the user to have configured the library for code would mean a promise
      kept only by accident.
- [x] A mixed-indentation file is **warned about, not repaired**. Tabs and spaces are invisible
      and identical on screen, so the typist has no way to know which the next line wants —
      but rewriting somebody's file would be editing the thing they wanted to practise.
      `ImportOutcome` carries warnings for the same reason: it is worth knowing before they
      wonder why the indentation will not match.
- [x] A minified line is reported. One line of forty thousand characters is technically
      importable and impossible to type; saying so at import beats leaving somebody to find out
      at the keyboard.
- [x] Comments are kept by default. They are prose written by a programmer and a large fraction
      of what anybody types in a working day; a file without them is not the file anybody works
      on. Stripping is string-literal aware, because the `//` in `"http://example.com"` starts
      no comment and cutting there cuts the line in half.
- [x] A block comment spanning lines takes the front of its continuation line with it. The
      first cut returned a prefix of each line, which is right until the comment ends
      mid-line — the code after `*/` was dropped and the comment before it kept, exactly
      backwards.
- [x] CRLF becomes LF without touching indentation: a `\r` is a line ending rather than
      trailing whitespace, and conflating them leaves a carriage return mid-text whenever
      trailing whitespace is kept.
- [x] Non-UTF-8 is refused with the byte offset. Checked in the extractor rather than left to
      normalisation because every other thing it reports is a statement about a text, and
      "line 40 mixes tabs and spaces" about a JPEG is a confident answer to the wrong question.
- [x] A language it cannot name still imports, with its whitespace intact and no sections.
      That is the right answer rather than a degraded one; guessing a section heuristic at it
      would be worse than declining to.
- [x] Section detection is a heuristic and says so. A top-level line opening a block in a
      braced language, `def`/`class` at column zero in Python. An indented definition belongs
      to the one above it, because a section per method puts a bookmark in the middle of a
      type.

---

## TX-004 — Subtitle extractor and directory import

**Type** feat · **Size** S · **Priority** P2 · **Depends on** TX-001

Subtitles are natural conversational prose with realistic punctuation, freely available, and
already cut into short lines — unusually good typing material.

**Scope**
- In: `.srt` and `.vtt`; strip indices, timecodes, and positioning tags; merge continuation
  lines into sentences; `DirectoryFetcher` importing a folder as one `text_item` per file.
- Out: styling and karaoke tags beyond stripping.

**Unit tests** (`SubtitleExtractorTest.cpp`, `DirectoryImportTest.cpp`)
- SRT and VTT fixtures extract to clean prose.
- Malformed timecodes are skipped with a warning, not fatal.
- HTML tags inside cues (`<i>`) are stripped.
- Duplicate consecutive cues (common in captions) are collapsed.
- Directory import skips unsupported files with a summary, rather than failing the whole batch.
- An empty directory reports clearly.

**Acceptance**
- [x] SRT and VTT extract to clean prose: no indices, no timecodes, no WebVTT header, no NOTE
      blocks, no cue identifiers, no positioning settings.
- [x] **Cues are re-joined into sentences.** A cue is wrapped to fit a screen, so its line
      breaks fall where the width ran out rather than where the sentence did — typing them as
      written is a test of the subtitler's line-wrapping and not of anybody's typing. A
      sentence spanning two cues comes back as one line; a closing quote after the full stop
      still ends it.
- [x] Tags inside a cue go: `<i>`, `<c.yellow>`, `{\an8}` and karaoke timestamps are
      instructions to a renderer, and typing `<i>` is typing markup that was never on screen.
      An *unclosed* bracket stays, because `Is a < b?` is a thing people say and eating the
      rest of the line would lose the dialogue.
- [x] A cue repeated verbatim in the very next slot is said once. Captions do this across a
      scene change more often than one would think, and a typing test that says the same
      sentence twice in a row reads as a bug in the test. A repeat that is *not* consecutive
      stays, because people do say the same thing twice in a conversation.
- [x] A malformed timecode skips its cue with a count and is not fatal. A subtitle file with
      one corrupt cue in nine hundred is a file worth importing; refusing the lot would be
      refusing the film. A file with no cues at all says so rather than importing silence.
- [x] Directory import reports rather than fails: one unreadable file in a folder of two
      hundred does not cost somebody the other hundred and ninety-nine, and the reason travels
      with the filename because a count of failures says something is wrong without saying
      what.
- [x] Subfolders are not recursed into, and the summary says why. Recursing would import a
      source tree's vendored dependencies from a single keystroke.
- [x] An empty folder is a clear error, and a file where a folder was expected is a *different*
      error from a path that is not there — "not a directory" about a missing path sends
      somebody looking for the wrong mistake.
- [x] Each file in a folder goes through its own extractor, so the Markdown arrives stripped
      and the source file arrives with its indentation, without anybody saying which was which.
- [x] `PlainTextExtractor` is down to `text/plain` alone. Markdown, code and subtitles were
      parked on it through TX-001 so that nothing which imported before the split stopped
      importing during it; each has taken its types back.

---

## TX-005 — Sections model

**Type** feat · **Size** M · **Priority** P1 · **Depends on** TX-002, TX-003 · **Docs** [TEXT_SOURCES §9](../TEXT_SOURCES.md#9-sections-and-schema)

`TextSection` as a first-class concept: a title and a grapheme range, so a bookmark can be per
chapter and the library can show "Chapter 4 of 31".

**Unit tests** (`TextSectionTest.cpp`)
- Section ranges are contiguous, non-overlapping, and cover the whole text.
- A text with no detected sections yields one implicit section covering everything.
- Grapheme offsets survive normalisation — sections are computed **after** normalisation, and a
  test proves offsets are not stale.
- Section titles are optional.

**Acceptance**
- [x] **Two types, not one.** An extractor knows byte offsets into the bytes it emitted, before
      normalisation; a bookmark is a grapheme offset into the text after it. Calling both
      `TextSection` was the setup for reading one as the other, so the extractor's is
      `SectionBoundary` and never leaves the pipeline. Import maps between them, through line
      numbers: every boundary either extractor produces sits at a line start, and normalisation
      is the one step here that promises to leave line structure alone.
- [x] Offsets are **not** the extractor's passed through, and a test says so by the amount they
      differ. Normalising `A line — with     spaces` shortens it by six characters and by eight
      bytes, which are not the same number; passing the byte offset on would put the marker for
      Chapter 4 inside Chapter 3 on any text with typography in it, which is every book.
- [x] The sections cover the text with no gap and no overlap, asserted by every case rather
      than by one, because it is an invariant of the function and not a property of one input.
      A bookmark is one number: a gap makes "which chapter is this" unanswerable, and an
      overlap gives it two answers, which is worse.
- [x] What comes before the first boundary is a section too — a source file's includes, a
      document's opening prose. The Markdown extractor already did this for itself; doing it
      once here covers the code extractor as well, whose first definition is rarely on line one.
- [x] A text with no structure has **one** section rather than none. The two describe the same
      text, and only one of them needs handling by everything downstream.
- [x] A boundary past the end, or out of order, is clamped rather than believed. Both are
      reachable — the first when normalisation drops a trailing heading — and neither should be
      able to produce a range the buffer cannot be indexed by.
- [x] An empty title is stored as no title. Empty and absent are the same thing said twice, and
      a section displayed as `""` is a blank where a chapter name goes.

---

## TX-006 — Schema v2 and section persistence

**Type** feat · **Size** M · **Priority** P1 · **Depends on** TX-005, TI-057 · **Docs** [TEXT_SOURCES §9](../TEXT_SOURCES.md#9-sections-and-schema)

`text_section` table; `section_idx` on bookmarks; `author`, `mime`, and `extractor` columns on
`text_item`.

This is the first schema migration after v1, and therefore the first live exercise of the
schema-version guard in [CI_CD §6](../CI_CD.md#6-tier-1--versioning-guards-version-guardyml).

**Unit tests** (`MigrationV3Test.cpp`)
- Migration from the previous version preserves every existing row.
- A pre-existing text with no sections gets its implicit single section.
- Existing bookmarks migrate to `section_idx = 0` with their offsets intact.
- Rollback on failure leaves `user_version` where it was.
- Cascade delete removes sections with their text.
- The CI schema guard **fails** if a released schema file is edited, or a new one is numbered at
  or below one — verified by the fixtures in `version-tools-test.sh`.

**Acceptance**
- [x] A user upgrading from beta keeps every imported text and every bookmark. Checked against
      a database built by replaying the *released* migration files rather than one this binary
      created, because a test that migrates a schema the binary just wrote is testing the
      binary against itself.
- [x] **It is `user_version` 3, not 2.** This issue and TEXT_SOURCES were written when v1 was
      the only schema; TI-109 took 2 for the daily-totals index in the meantime. The number
      comes from the filename, and the only rule that matters is that a new file is above every
      released one — which is precisely what the CI guard checks, and what a file numbered 2
      would have silently failed: every database already past 2 would skip it.
- [x] A text that was in the library before today gets the one section it always implicitly
      had, backfilled by the migration. Without it, every reader would need an "or none, for
      the old ones" branch — which is the branch TX-005 exists to remove.
- [x] `section_idx` is `NOT NULL DEFAULT 0` rather than nullable, and the default is what
      backfills the bookmarks already on disk. Every text has a section covering all of it, so
      "no section" describes nothing; a nullable column would be a null every reader defends
      against and no writer can produce.
- [x] The new `text_item` columns arrive **empty** on an upgraded database. A text imported
      before the pipeline recorded its type has no type to record, and guessing one from the
      origin's extension would be writing down a fact nobody established.
- [x] A failed upgrade leaves the version, the rows and the columns exactly as they were —
      including the `ALTER`s, which go back with the rest of the step. The next start retries
      this migration rather than the one after it, and the rows are still there to retry
      against.
- [x] A text and its sections are written in one transaction. A text with half its sections is
      one where "which chapter is this offset in" has no answer, and the migration made sure
      that state does not otherwise exist.
- [x] `extractor` is recorded because the first time somebody reports that a file imported
      wrongly, the answer to "which of eight extractors produced this" is otherwise a guess
      from the filename.
- [x] The bookmark's section is worked out by the service at write time, not by whoever
      displays it. A section index derived at read time disagrees with its offset the moment a
      text is re-imported, and this is written once per run.
- [x] The fake and the SQLite repository are held to the same contract for all of it, because
      a fake that forgets the sections is a service that passes its tests and loses chapters.

---

## TX-007 — Typing-readiness pass

**Type** feat · **Size** L · **Priority** P1 · **Depends on** TX-005 · **Docs** [TEXT_SOURCES §8](../TEXT_SOURCES.md#8-typing-readiness)

The difference between a feature and a trap. An extracted chapter contains hyphenated line
breaks, page numbers, footnote markers, and typography no keyboard produces — and the current
application would simply mark every attempt at `—` wrong, forever.

**Scope**
- In: dehyphenation with a dictionary check; running-head and page-number removal; footnote
  marker stripping; table-of-contents detection; paragraph rejoining; Gutenberg boilerplate
  detection; a **pre-import report of every grapheme unreachable on a standard keyboard, with
  counts**.
- Out: OCR error correction.

**Unit tests** (`TypingReadinessTest.cpp`) — table-driven, each step individually toggleable
- `exam-\nple` → `example`; `well-\nknown` stays hyphenated (dictionary check works).
- Repeated short lines matching the title are dropped; a genuine repeated sentence is not.
- Footnote markers `[12]` and superscripts are stripped; a genuine `[1]` in code is not (code
  mode disables this step).
- Hard-wrapped paragraphs rejoin; a deliberate line break in poetry is preserved (documented
  heuristic and its limits).
- A Gutenberg header/footer fixture is detected and offered for removal.
- The unreachable-character report counts `—`, `"`, `…`, and non-Latin scripts correctly.
- **Idempotence**: running the pass twice equals running it once.
- Every step disableable, verified by a toggle matrix.

**Acceptance**
- [ ] The unreachable-character report appears **before** import completes, with the option to
      flatten or cancel.
- [ ] Idempotence holds across all toggle combinations.

---

## TX-008 — EPUB extractor

**Type** feat · **Size** L · **Priority** P1 · **Depends on** TX-006, TX-007

**Scope**
- In: miniz for the ZIP container; parse `META-INF/container.xml` → OPF; read spine order;
  extract each XHTML document in order; chapters become sections; title, author, and language
  from OPF metadata; EPUB 2 and 3.
- Out: DRM (refuse with a clear message), fixed-layout EPUBs (detect and warn), images.

**Unit tests** (`EpubExtractorTest.cpp`) — fixtures committed, including deliberately broken ones
- A valid EPUB 2 and EPUB 3 extract with chapters in **spine order**, not archive order.
- Metadata populates title, author, and language.
- A DRM-protected file is refused with an explanatory message.
- A corrupt ZIP, a missing `container.xml`, and a missing OPF each fail cleanly.
- A ZIP bomb is refused — decompressed-size and entry-count limits enforced **during**
  extraction, not after.
- Path traversal in an archive entry (`../../etc/passwd`) is refused. This is a real and
  well-known archive attack.
- An EPUB with no spine, or an empty one, reports rather than producing an empty text.
- Non-ASCII metadata and content round-trip.

**Acceptance**
- [ ] Archive-security tests (bomb, traversal) pass. This is the first code in the project that
      parses a container format from an untrusted source, and it is treated as such.
- [ ] Added to the fuzz targets in CI-015.

---

## TX-009 — External converter hook

**Type** feat · **Size** M · **Priority** P1 · **Depends on** TX-001 · **Docs** [TEXT_SOURCES §7](../TEXT_SOURCES.md#7-the-external-converter-hook), [ADR-015](../ARCHITECTURE.md#adr-015--heavy-formats-use-an-external-converter-hook-not-a-bundled-parser)

`ExternalCommandExtractor` running a user-configured command. `pandoc` alone covers roughly
forty formats; inheriting them for nothing beats bundling any one parser.

**Scope**
- In: `[import.converters]` config; `{input}`/`{output}` substitution; argv array execution
  (**never a shell**); timeout; output size limit; temp file cleanup; `--doctor` reporting which
  configured converters are actually installed.
- Out: shipping any converter.

**Unit tests** (`ExternalConverterTest.cpp`)
- A configured converter runs and its stdout is captured.
- The output-file form works as well as the stdout form.
- A missing binary reports "converter not installed", naming it — not a generic failure.
- A non-zero exit code is reported with the converter's stderr.
- A converter that hangs is killed at the timeout.
- Output exceeding the limit is truncated and reported.
- **A filename containing shell metacharacters (`; rm -rf ~`) is passed through safely** —
  argv execution, never a shell. Explicit test.
- The command is read **only** from config: a test proves that content, filenames, and URLs
  cannot influence it.
- Temp files are removed on success, on failure, and on timeout.
- The catch-all `"*"` converter is tried only after every built-in extractor declines.

**Acceptance**
- [ ] Command injection is impossible by construction, and there is a test that would catch a
      regression to shell execution.

---

## TX-010 — HTTP fetcher

**Type** feat · **Size** L · **Priority** P2 · **Depends on** TX-001 · **Docs** [TEXT_SOURCES §6](../TEXT_SOURCES.md#6-network-access), [ADR-014](../ARCHITECTURE.md#adr-014--network-access-is-optional-at-build-time-and-opt-in-at-runtime)

The only feature in TypeIt that touches a network, and the only one that needs its own rules.

**Scope**
- In: `TYPEIT_ENABLE_NETWORK` build option (with it off, libcurl is not a dependency at all);
  `[network] enabled = false` default; first-use confirmation; HTTPS-only by default; redirect
  limit 5 with no scheme downgrade; 10 s connect / 30 s total timeout; 10 MB streaming size cap;
  identifiable User-Agent; `robots.txt` honoured; private and loopback addresses refused unless
  explicitly allowed.
- Out: link following of any kind — see below.

**Unit tests** (`HttpFetcherTest.cpp`) — against a local test server, never the live internet
- A successful fetch captures content, MIME from `Content-Type`, and a title.
- HTTP is refused unless opted in; an HTTPS→HTTP redirect is refused always.
- Redirect chains terminate at the limit; a redirect loop is detected.
- A slow server hits the timeout and reports it.
- A response exceeding the size cap is aborted **during streaming**, not after buffering it.
- `robots.txt` disallow is honoured; a missing `robots.txt` is treated as allowed.
- `localhost`, `127.0.0.1`, `169.254.x.x`, and RFC 1918 addresses are refused by default.
- With `enabled = false`, any URL import is refused before a socket is opened.
- With `TYPEIT_ENABLE_NETWORK=OFF`, the binary contains no networking symbols and `--doctor`
  reports the feature as unavailable.
- **The fetcher exposes no way to enqueue a second URL.** A test asserts the interface takes one
  locator and returns one result.

**Acceptance**
- [ ] Both build configurations are exercised in CI.
- [ ] TypeIt makes no request the user did not initiate: no telemetry, no update check, no
      analytics. Verifiable from the link line, not just from a promise in a README.
- [ ] The no-crawling boundary is enforced by the interface shape, not by convention.

---

## TX-011 — HTML readability extractor

**Type** feat · **Size** L · **Priority** P2 · **Depends on** TX-010 · **Docs** [TEXT_SOURCES §5](../TEXT_SOURCES.md#5-html-extraction)

Naive tag-stripping yields navigation menus and cookie banners. This implements the
density-scoring heuristic in TEXT_SOURCES §5.

**Scope**
- In: DOM parse; chrome removal; text-density scoring; main-subtree selection; plain-text
  conversion preserving paragraphs, line breaks, list markers, and `<pre>`; title extraction;
  a **preview with an implausibly-little-text warning**.
- Out: JavaScript execution — TypeIt does not run JS and will not.

**Unit tests** (`HtmlExtractorTest.cpp`) — committed fixtures from representative real pages
- An article page extracts the article and none of the navigation.
- A documentation page extracts the content and not the sidebar.
- A page that is mostly navigation triggers the low-yield warning.
- `<pre>` content keeps its whitespace.
- Malformed HTML (unclosed tags, mismatched nesting) does not crash.
- A deeply nested document does not blow the stack — explicit depth limit.
- Character references and encoding declarations are handled; a non-UTF-8 charset is decoded or
  refused clearly.
- An empty body reports rather than importing nothing.

**Acceptance**
- [ ] Extraction quality is measured against the fixture set, and the measured accuracy is
      recorded in the issue — including where it is poor.
- [ ] JS-rendered pages fail **honestly**: the user sees a preview and can reject it. Silently
      importing a nav menu as a typing test would be worse than refusing.
- [ ] Added to the fuzz targets in CI-015.

---

## TX-012 — Web import UX and CLI

**Type** feat · **Size** M · **Priority** P2 · **Depends on** TX-011, TI-118

**Scope**
- In: URL entry in the library screen; a fetch progress indicator; an extraction preview with
  accept/reject; `typeit --url <URL>`; a curated bookmark list of public-domain sources
  (Project Gutenberg, Wikisource) shipped as a starting point.
- Out: a browser, a bookmark manager, anything resembling a reader.

**Unit tests** (`WebImportTest.cpp`)
- The preview shows extracted text before anything is stored.
- Rejecting stores nothing at all.
- A fetch failure surfaces a specific, actionable message.
- A long fetch can be cancelled, and cancelling cleans up.
- Re-importing a URL already in the library offers the existing entry rather than refetching.
- The first URL import prompts for consent exactly once, and the choice persists.

---

## Phase exit criteria

**Waves 1–2 (in `v2.0.0-beta.1`)**
- [ ] Markdown, code, and subtitle files import cleanly.
- [ ] Sections work end to end; an ebook-sized text is typed chapter by chapter with per-chapter
      bookmarks.
- [ ] Schema v2 migrates without data loss, and the CI schema guard has been watched to fail.
- [ ] The typing-readiness pass reports unreachable characters before import.

**Waves 3–4 (in `v2.1.0`)**
- [ ] A real EPUB imports with correct chapters, metadata, and readable text.
- [ ] Archive-security tests pass: ZIP bomb and path traversal both refused.
- [ ] The external converter hook handles PDF via `pdftotext` and arbitrary formats via
      `pandoc`, with command injection impossible by construction.
- [ ] A web article imports with the article text and none of the chrome.
- [ ] `TYPEIT_ENABLE_NETWORK=OFF` produces a binary with no networking symbols.
- [ ] Every new parser is in the fuzz corpus.
