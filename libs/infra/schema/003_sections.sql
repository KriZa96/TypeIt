-- Sections, and the three things a text remembers about how it got here
-- (TX-006, TEXT_SOURCES section 9).
--
-- TEXT_SOURCES calls this "schema v2" because it was written when v1 was the
-- only one. It is version 3: TI-109 took 2 for the daily-totals index. The
-- number comes from the filename and the migrator applies whatever it finds
-- above the database's own version, so the only thing that matters is that this
-- is above everything already released.
--
-- The first migration after the schema shipped, and therefore the first live
-- exercise of the guard in CI_CD section 6: a released file is never edited,
-- and a new one is numbered above every released one.

-- A chapter, an article, a function. Grapheme offsets, because that is what a
-- bookmark is measured in and comparing an offset against a range should not
-- need a conversion first (TX-005).
--
-- WITHOUT ROWID: the primary key *is* the row here, and the alternative is a
-- hidden rowid plus an index over the key, which is the same data stored twice
-- for a table nobody looks up any other way.
CREATE TABLE text_section (
    text_id   INTEGER NOT NULL REFERENCES text_item(id) ON DELETE CASCADE,
    idx       INTEGER NOT NULL,
    title     TEXT,                -- absent where the format gave no name
    start_idx INTEGER NOT NULL,
    end_idx   INTEGER NOT NULL,    -- exclusive, and equal to the next start
    PRIMARY KEY (text_id, idx)
) WITHOUT ROWID;

-- Which section the bookmark is in. NOT NULL with a default rather than
-- nullable: every text has at least one section covering all of it, so "no
-- section" describes nothing, and a nullable column would be a null every
-- reader has to defend against for a case that cannot arise. The default is
-- also what backfills the bookmarks that are already here — section zero, with
-- their offsets untouched.
ALTER TABLE text_bookmark ADD COLUMN section_idx INTEGER NOT NULL DEFAULT 0;

-- Where it came from and what read it. `extractor` earns its place the first
-- time somebody reports that a file imported wrongly: the answer to "which one
-- of eight extractors produced this" is otherwise a guess from the filename.
ALTER TABLE text_item ADD COLUMN author    TEXT;
ALTER TABLE text_item ADD COLUMN mime      TEXT;
ALTER TABLE text_item ADD COLUMN extractor TEXT;

-- Every text already in the library gets the one section it always implicitly
-- had. Without this, a library imported before today would have texts with no
-- sections at all, and every reader would need a "or none, for the old ones"
-- branch — which is exactly the branch TX-005 exists to remove.
INSERT INTO text_section (text_id, idx, title, start_idx, end_idx)
SELECT id, 0, NULL, 0, grapheme_count FROM text_item;
