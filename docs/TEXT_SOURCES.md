# TypeIt — Text Ingestion Architecture

*How arbitrary content becomes typeable text: files, web pages, ebooks, subtitles, source code,
and anything a converter on the user's machine can handle.*

The original `ITextSource` was the right instinct — a one-method seam for "where does text come
from". This document extends that instinct into a pipeline capable of handling formats the
original never anticipated, without any of the layers knowing about each other.

---

## 1. Two different problems

The current design conflates them, and separating them is the whole design:

| | **Ingestion** | **Supply** |
|---|---|---|
| Question | Where does content come from, and how does it become plain text? | During a run, what text does the typist get next? |
| Frequency | Once, at import | Continuously, during play |
| Interface | `IContentFetcher` + `ITextExtractor` | `ITextProvider` |
| Layer | `app` orchestrates, `infra` implements | `core`, pure |
| Output | A normalised `text_item` in the library | A stream of graphemes |

`ITextProvider` ([GAMEPLAY §5.4](GAMEPLAY.md#54-providers)) stays exactly as designed and is
unaffected by any of this. Once content is in the library, endless mode does not care whether
it came from a file or a website.

---

## 2. The pipeline

```
   ┌──────────────┐   bytes    ┌──────────────┐   plain    ┌──────────────┐  normalised
   │  Acquisition │──────────► │  Extraction  │──────────► │Normalisation │─────────────►  Library
   │IContentFetcher│  + MIME   │ITextExtractor│   text     │TextNormalizer│                (SQLite)
   └──────────────┘            └──────────────┘  +sections └──────────────┘
      file, stdin,               txt, md, html,              line endings,
      paste, URL,                epub, srt, code,            NFC, typography,
      directory                  docx, external              whitespace, tabs
```

Each stage is independently testable and independently extensible. Adding EPUB support means
writing one extractor; adding web support means writing one fetcher. Neither touches the other,
and neither touches `core`.

---

## 3. Acquisition — `IContentFetcher`

```cpp
struct FetchedContent {
    std::vector<std::byte> bytes;
    std::string            detected_mime;   // from headers, magic bytes, or extension
    std::string            origin;          // path or URL, recorded in text_item.origin
    std::string            suggested_title; // filename, HTML <title>, EPUB metadata
};

class IContentFetcher {
public:
    virtual ~IContentFetcher() = default;
    [[nodiscard]] virtual bool can_handle(std::string_view locator) const = 0;
    [[nodiscard]] virtual Result<FetchedContent> fetch(std::string_view locator) const = 0;
};
```

| Fetcher | Locator | Notes |
|---|---|---|
| `FileFetcher` | a path | Size limit; MIME by magic bytes then extension |
| `StdinFetcher` | `-` | Must reattach the TUI to `/dev/tty` / `CONIN$` afterwards |
| `PasteFetcher` | in-app | Handles bracketed paste as one event |
| `HttpFetcher` | `http(s)://…` | **Optional at build time, opt-in at runtime** — see §6 |

A folder is deliberately *not* on that list. `fetch` returns one `FetchedContent`, and a folder
of two hundred files is two hundred texts with two hundred MIME types and two hundred ways to
fail — a shape the interface cannot express without becoming a list. Batch import is therefore
one operation on the importing service, which runs each child down the ordinary single-file
path and reports a summary rather than failing on the first bad file (TX-004).

---

## 4. Extraction — `ITextExtractor`

```cpp
struct ExtractedText {
    std::string              text;
    std::vector<SectionBoundary> sections;  // chapters, articles, functions — may be empty
    std::optional<std::string> title;
    std::optional<std::string> author;
    std::optional<std::string> language;
};

class ITextExtractor {
public:
    virtual ~ITextExtractor() = default;
    [[nodiscard]] virtual std::span<const std::string_view> mime_types() const = 0;
    [[nodiscard]] virtual Result<ExtractedText> extract(const FetchedContent&) const = 0;
};
```

Extractors register with an `ExtractorRegistry` keyed by MIME type; resolution is a user
override first, then content sniffing, then the extension. That is descending order of trust,
not a fallback chain: an override is somebody looking at the file and saying what it is, which
beats a signature, which beats a name anybody can mistype. Two extractors claiming one type is
a startup error rather than last-wins, because a silent overwrite lets the link order decide
which one runs and the symptom is an EPUB extracted as a ZIP on one machine and correctly on
another. An unknown format falls through to the
external-converter hook (§7) before it is refused.

### Built-in extractors

| Format | Extractor | Dependency | Behaviour |
|---|---|---|---|
| `text/plain` | `PlainTextExtractor` | none | Passthrough |
| `text/markdown` | `MarkdownExtractor` | none | Strips or keeps markup (configurable); headings become sections |
| `text/html` | `HtmlExtractor` | lexbor or pugixml | Readability-style main-content extraction |
| `application/epub+zip` | `EpubExtractor` | miniz + XML parser | Spine order; each chapter a section |
| `text/x-subrip`, `text/vtt` | `SubtitleExtractor` | none | Strips timecodes and indices — leaves natural dialogue |
| source code | `CodeExtractor` | none | Preserves indentation and symbols; functions become sections |
| `application/vnd.openxmlformats…docx` | `DocxExtractor` | miniz + XML parser | `word/document.xml` paragraph text |
| anything else | `ExternalCommandExtractor` | none (uses the user's tools) | §7 |

### Why subtitles and code are on that list

Neither is an obvious "document format", and both are genuinely good typing material.
Subtitles are natural conversational prose with realistic punctuation, freely available, and
already cut into short lines. Source code is what a large share of this application's likely
users type all day — and it exercises symbols, brackets, and indentation that prose never
touches. `CodeExtractor` therefore preserves whitespace exactly, and `[typing] strict_spaces`
matters much more in code mode than in prose.

---

## 5. HTML extraction

Naively stripping tags produces navigation menus, cookie banners, and footers. The extractor
implements a Readability-style heuristic:

1. Parse to a DOM.
2. Drop `script`, `style`, `nav`, `header`, `footer`, `aside`, `form`, and elements whose
   class or id matches known chrome patterns (`comment`, `sidebar`, `promo`, `share`).
3. Score remaining block elements by text density — characters of text divided by tag count —
   with paragraph count and comma count as positive signals.
4. Take the highest-scoring subtree plus its siblings above a threshold.
5. Convert to plain text: block elements become paragraph breaks, `<br>` a line break, list
   items get a marker, `<pre>` keeps its whitespace.
6. Take the title from `<h1>` inside the chosen subtree, falling back to `<title>`.

**Being honest about accuracy:** this heuristic works well on articles and documentation and
poorly on single-page applications that render text with JavaScript. TypeIt does not execute
JavaScript and will not. When extraction yields implausibly little text, the import reports
this and offers a preview so the user can reject it — silently importing a navigation menu as a
typing test would be worse than refusing.

---

## 6. Network access

Fetching from the web is the only feature in TypeIt that touches a network, so it gets explicit
rules.

**Build time.** `TYPEIT_ENABLE_NETWORK` (default `ON`, but genuinely optional). With it off,
`HttpFetcher` is not compiled and libcurl is not a dependency — distro packagers and
minimalists get a strictly local application, and the feature's absence is reported cleanly by
`--doctor` rather than failing mysteriously.

**Runtime.** `[network] enabled = false` by default. A URL import prompts for confirmation the
first time. TypeIt makes **no network request the user did not initiate**: no telemetry, no
update checks, no analytics, ever.

**Behaviour of a fetch:**

| Rule | Value |
|---|---|
| Scheme | HTTPS only by default; HTTP requires an explicit opt-in |
| Redirects | Followed, maximum 5, scheme downgrade refused |
| Timeout | 10 s connect, 30 s total |
| Size limit | 10 MB, enforced during streaming, not after |
| User-Agent | `TypeIt/<version> (+repository URL)` — identifiable, not spoofed |
| `robots.txt` | Honoured. A single user-initiated fetch arguably needn't, but the cost is one request and the alternative is arguing about it |
| Link following | **None.** TypeIt fetches exactly one URL. It is not a crawler and must never become one |
| Private addresses | `localhost`, link-local, and RFC 1918 ranges refused unless explicitly allowed |
| Caching | Content is stored in the library; a URL is fetched once |

**Content ownership.** Fetched content stays on the user's machine. TypeIt has no server and
uploads nothing. Users are responsible for what they import; the documentation says so once,
plainly, and points at Project Gutenberg as a large source of unambiguously public-domain
books.

---

## 7. The external converter hook

Bundling a PDF parser means bundling poppler or pdfium — tens of megabytes, a significant
attack surface, and a build burden on both platforms, in exchange for text extraction that is
mediocre anyway because PDF is a page-description format that does not really contain
paragraphs.

The better trade is an escape hatch:

```toml
[import.converters]
"application/pdf"  = "pdftotext -layout -nopgbrk {input} -"
"application/epub+zip" = "ebook-convert {input} {output}.txt"   # overrides the built-in
"*"                = "pandoc --to=plain --wrap=none {input}"    # catch-all fallback
```

`ExternalCommandExtractor` writes the fetched bytes to a temp file, runs the configured
command, and reads plain text back from stdout or the output file.

This is a good trade because it is nearly free, and because `pandoc` alone covers something
like forty formats — TypeIt inherits all of them without writing a parser or shipping a
dependency.

**Security.** The command comes **only** from the user's own configuration file. It is never
derived from imported content, from a filename, or from a URL. Arguments are passed as an argv
array, never through a shell. The `{input}` and `{output}` placeholders are the only
substitutions. Timeouts and output size limits apply. `--doctor` reports which configured
converters are actually present on the system.

---

## 8. Typing readiness

An EPUB chapter is not a typing test. It contains front matter, page numbers, footnote markers,
words hyphenated across line breaks, and typography no keyboard produces. A pass between
extraction and normalisation cleans this up:

| Step | Default | What it does |
|---|---|---|
| Dehyphenation | on | `exam-\nple` → `example`, checked against the document's own vocabulary to avoid mangling genuine hyphens |
| Drop running heads | on | Repeated short lines and page-number lines |
| Footnote markers | on | Strips `[12]` and superscript markers |
| Table of contents | on | Detects and drops leading dotted-leader blocks |
| Paragraph rejoin | on | Reflows hard-wrapped lines into paragraphs — the wrapping is TypeIt's job |
| Front/back matter | on, reported | Removes legal boilerplate (Gutenberg headers are formulaic and detectable) |
| Non-typeable characters | report | Reports any grapheme unreachable on a standard keyboard, with a count, before import |

**On the dictionary.** There is no bundled word list. The document is the dictionary: the pass
takes the words the text itself uses, and decides a line-break hyphen in descending order of
evidence — the document spells the word joined somewhere else, it spells it hyphenated
somewhere else, or both halves are words the document uses on their own. Failing all three the
hyphen goes, because a hyphen landing exactly at a line ending is far more often a typesetter's
than an author's. A bundled list would be one language's, would need generating and guarding
like the Unicode tables, and would still be wrong about the names and jargon that most line
breaks land in.

**On the prompt.** Front matter is removed rather than prompted for, and the removal is in the
report. There is no prompt at this layer to hand it to; the original bytes are kept in
`content_raw`; and eight kilobytes of licence in a typing test is a worse outcome than a line
in a report. `inspect_file` produces the same report *before* anything is stored, which is
where a caller that wants to ask gets its answer.

Every step is individually toggleable and the whole pass is off for source code, where each
step would be actively wrong: dehyphenation joins `foo-\nbar`, paragraph rejoin puts a function
on one line, `[1]` is a subscript rather than a footnote, and a line that is nothing but a
number is a value rather than a page.

The pass is idempotent, across every combination of the toggles, because re-importing a text
after changing a setting is exactly running it twice.

The last row matters most. Importing text containing `—`, `"`, or `…` produces a test the user
cannot pass, and the current application would simply mark every attempt wrong forever.
Reporting it up front — and flattening it by default
([GAMEPLAY §5.2](GAMEPLAY.md#52-normalisation-at-import)) — is the difference between a feature
and a trap.

The report counts each unreachable grapheme and says, for each, **whether normalisation will
rescue it**. An em dash becomes a hyphen and stops being a problem; `č` does not. A single
number would hide both: judged against ASCII — because TypeIt does not know the keyboard in
front of the user — a page of Croatian looks as alarming as a page of smart quotes, and only
one of the two is actually a trap. `ReadinessReport::unreachable_after_normalisation()` is the
figure worth showing somebody before they agree to type a text.

---

## 9. Sections and schema

Long content needs structure. `TextSection` records a title and a grapheme range, which lets
the bookmark be per chapter, the library show "Chapter 4 of 31", and a session target one
chapter rather than an arbitrary offset.

Two types rather than one, because an extractor and a bookmark measure different things. A
`SectionBoundary` is what an extractor found, in bytes of its own output and before
normalisation ran — which is everything it can honestly know. A `TextSection` is graphemes into
the normalised text, which is the unit a bookmark is in. Import maps the first onto the second,
through line numbers: every boundary either extractor produces sits at the start of a line, and
normalisation is the one step here that promises to leave line structure alone. Passing the
byte offsets through unchanged would put the marker for Chapter 4 inside Chapter 3 on any text
with typography in it, which is every book.

The sections of a text are contiguous, non-overlapping, and cover it end to end. A bookmark is
one number, so wherever it lands it must land in exactly one section: a gap makes "which
chapter is this" unanswerable and an overlap gives it two answers. A text nobody found
structure in therefore has *one* section covering all of it rather than none — the two describe
the same text, and only one of them needs handling everywhere downstream.

```sql
-- 003_sections.sql
CREATE TABLE text_section (
    text_id   INTEGER NOT NULL REFERENCES text_item(id) ON DELETE CASCADE,
    idx       INTEGER NOT NULL,
    title     TEXT,                -- absent where the format gave no name
    start_idx INTEGER NOT NULL,    -- grapheme offset
    end_idx   INTEGER NOT NULL,    -- exclusive, and equal to the next start
    PRIMARY KEY (text_id, idx)
) WITHOUT ROWID;

ALTER TABLE text_bookmark ADD COLUMN section_idx INTEGER NOT NULL DEFAULT 0;
ALTER TABLE text_item     ADD COLUMN author    TEXT;
ALTER TABLE text_item     ADD COLUMN mime      TEXT;
ALTER TABLE text_item     ADD COLUMN extractor TEXT;   -- which one produced this; aids debugging

-- Every text already in the library gets the one section it always implicitly had.
INSERT INTO text_section (text_id, idx, title, start_idx, end_idx)
SELECT id, 0, NULL, 0, grapheme_count FROM text_item;
```

`section_idx` is `NOT NULL` with a default rather than nullable, and the default is also what
backfills the bookmarks already on disk. Every text has at least one section covering all of
it, so "no section" describes nothing — a nullable column would be a null that every reader
defends against and no writer can produce.

This is user_version **3**, not 2: this document was written when v1 was the only schema, and
TI-109 took 2 for the daily-totals index. It is the first migration after the schema shipped,
and therefore the first live exercise of the guard in
[CI_CD §6](CI_CD.md#6-tier-1--versioning-guards-version-guardyml): a released schema file is
never edited, and a new one must be numbered above every released one.

---

## 10. Extending it

Adding a format should require touching nothing that already works:

| To add | Do this |
|---|---|
| A new file format | Implement `ITextExtractor`, register its MIME types, add a fixture and tests |
| A new acquisition route | Implement `IContentFetcher`, register it |
| A format you do not want to write a parser for | Add a line to `[import.converters]` |
| A new readability heuristic | It is contained in `HtmlExtractor`; nothing else knows |

No change to `core`, no change to `ITextProvider`, no change to any mode. That is the payoff
for separating ingestion from supply in §1.

---

## 11. Sequencing

Plain-text import lands in Phase 6 (TI-110 – TI-119). This document's pipeline is
[Phase 6A](issues/PHASE-6A-text-sources.md):

| Wave | Content | Issues | Ships in |
|---|---|---|---|
| **1** | The fetch/extract split, Markdown, code, subtitles, directory import | TX-001 – TX-004 | `2.0.0-beta.1` |
| **2** | Sections, schema v2, typing-readiness pass | TX-005 – TX-007 | `2.0.0-beta.1` |
| **3** | EPUB, external converter hook | TX-008 – TX-009 | `2.1.0` |
| **4** | HTTP fetching, HTML readability | TX-010 – TX-012 | `2.1.0` |

Web fetching is last deliberately. It carries the only new runtime dependency, the only network
surface, and the fuzziest correctness criterion in the project. Everything before it is
offline, deterministic, and cheap to test — and each wave is independently useful, so the
feature can stop after any of them without leaving something half-built.
