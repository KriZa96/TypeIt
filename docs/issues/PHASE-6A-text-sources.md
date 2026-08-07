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
- [ ] Code mode interacts correctly with `strict_spaces`, which matters far more here than in
      prose.

---

## TX-004 — Subtitle extractor and directory import

**Type** feat · **Size** S · **Priority** P2 · **Depends on** TX-001

Subtitles are natural conversational prose with realistic punctuation, freely available, and
already cut into short lines — unusually good typing material.

**Scope**
- In: `.srt` and `.vtt`; strip indices, timecodes, and positioning tags; merge continuation
  lines into sentences; `DirectoryFetcher` importing a folder as one `text_item` per file.
- Out: styling and karaoke tags beyond stripping.

**Unit tests** (`SubtitleExtractorTest.cpp`, `DirectoryFetcherTest.cpp`)
- SRT and VTT fixtures extract to clean prose.
- Malformed timecodes are skipped with a warning, not fatal.
- HTML tags inside cues (`<i>`) are stripped.
- Duplicate consecutive cues (common in captions) are collapsed.
- Directory import skips unsupported files with a summary, rather than failing the whole batch.
- An empty directory reports clearly.

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

---

## TX-006 — Schema v2 and section persistence

**Type** feat · **Size** M · **Priority** P1 · **Depends on** TX-005, TI-057 · **Docs** [TEXT_SOURCES §9](../TEXT_SOURCES.md#9-sections-and-schema)

`text_section` table; `section_idx` on bookmarks; `author`, `mime`, and `extractor` columns on
`text_item`.

This is the first schema migration after v1, and therefore the first live exercise of the
schema-version guard in [CI_CD §6](../CI_CD.md#6-tier-1--versioning-guards-version-guardyml).

**Unit tests** (`MigrationV2Test.cpp`)
- Migration from v1 to v2 preserves every existing row.
- A pre-existing text with no sections gets its implicit single section.
- Existing bookmarks migrate to `section_idx = 0` with their offsets intact.
- Rollback on failure leaves `user_version` at 1.
- Cascade delete removes sections with their text.
- The CI schema guard **fails** if `user_version` is not bumped — verified by deliberately
  omitting the bump in a scratch branch.

**Acceptance**
- [ ] A user upgrading from beta keeps every imported text and every bookmark.

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
