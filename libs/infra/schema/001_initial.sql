-- Schema v1 (TECHNICAL section 5).
--
-- Applied by the Migrator, which owns `PRAGMA user_version` and sets it after
-- this file succeeds. The file does not set it itself: a script that stamped
-- its own version could stamp it before failing halfway.
--
-- Never edited once released. A change to a shipped schema is a new file with
-- the next number and a migration test from the previous one (VERSIONING
-- section 5), which the CI schema guard enforces.

CREATE TABLE profile (
    id            INTEGER PRIMARY KEY CHECK (id = 1),
    created_at    INTEGER NOT NULL,
    display_name  TEXT,
    daily_goal_ms INTEGER NOT NULL DEFAULT 600000
);

CREATE TABLE text_item (
    id             INTEGER PRIMARY KEY,
    title          TEXT    NOT NULL,
    source         TEXT    NOT NULL CHECK (source IN ('builtin','file','paste','stdin')),
    origin         TEXT,
    content        TEXT    NOT NULL,    -- normalised
    content_raw    TEXT,                -- as imported
    content_sha256 TEXT    NOT NULL UNIQUE,
    language       TEXT,
    grapheme_count INTEGER NOT NULL,
    word_count     INTEGER NOT NULL,
    difficulty     REAL,
    created_at     INTEGER NOT NULL
);

CREATE TABLE text_tag (
    text_id INTEGER NOT NULL REFERENCES text_item(id) ON DELETE CASCADE,
    tag     TEXT    NOT NULL,
    PRIMARY KEY (text_id, tag)
);

CREATE TABLE text_bookmark (
    text_id    INTEGER PRIMARY KEY REFERENCES text_item(id) ON DELETE CASCADE,
    offset     INTEGER NOT NULL,
    updated_at INTEGER NOT NULL
);

CREATE TABLE session (
    id                 INTEGER PRIMARY KEY,
    started_at         INTEGER NOT NULL,       -- unix ms
    ended_at           INTEGER NOT NULL,
    mode               TEXT    NOT NULL,
    mode_param         TEXT,                   -- JSON: {"seconds":30} / {"words":50} / race params
    text_id            INTEGER REFERENCES text_item(id) ON DELETE SET NULL,
    provider           TEXT    NOT NULL,
    provider_seed      INTEGER NOT NULL,       -- reproduces the exact text stream
    duration_ms        INTEGER NOT NULL,
    graphemes_typed    INTEGER NOT NULL,
    graphemes_correct  INTEGER NOT NULL,
    errors_total       INTEGER NOT NULL,
    errors_uncorrected INTEGER NOT NULL,
    backspaces         INTEGER NOT NULL,
    raw_wpm            REAL    NOT NULL,
    gross_wpm          REAL    NOT NULL,
    net_wpm            REAL    NOT NULL,
    accuracy           REAL    NOT NULL,
    final_correctness  REAL    NOT NULL,
    consistency        REAL    NOT NULL,
    peak_wpm           REAL,                   -- race: highest sustained pacer speed
    wall_wpm           REAL,                   -- race: speed at which accuracy collapsed
    completed          INTEGER NOT NULL CHECK (completed IN (0,1)),
    app_version        TEXT    NOT NULL
);
CREATE INDEX idx_session_started ON session(started_at DESC);
CREATE INDEX idx_session_mode    ON session(mode, started_at DESC);

CREATE TABLE session_sample (          -- per-second timeline for charts
    session_id INTEGER NOT NULL REFERENCES session(id) ON DELETE CASCADE,
    t_ms       INTEGER NOT NULL,
    wpm        REAL    NOT NULL,
    errors     INTEGER NOT NULL,
    pacer_wpm  REAL,
    PRIMARY KEY (session_id, t_ms)
) WITHOUT ROWID;

CREATE TABLE key_stat (                -- lifetime aggregate, merged per session
    grapheme         TEXT    PRIMARY KEY,
    attempts         INTEGER NOT NULL,
    errors           INTEGER NOT NULL,
    total_latency_ms INTEGER NOT NULL
);

CREATE TABLE bigram_stat (
    bigram           TEXT    PRIMARY KEY,
    attempts         INTEGER NOT NULL,
    errors           INTEGER NOT NULL,
    total_latency_ms INTEGER NOT NULL
);

CREATE TABLE error_pair (
    expected TEXT    NOT NULL,
    typed    TEXT    NOT NULL,
    count    INTEGER NOT NULL,
    PRIMARY KEY (expected, typed)
);

CREATE TABLE personal_best (
    mode        TEXT    NOT NULL,
    param       TEXT    NOT NULL,
    metric      TEXT    NOT NULL,       -- 'net_wpm' | 'peak_wpm' | 'accuracy'
    session_id  INTEGER NOT NULL REFERENCES session(id) ON DELETE CASCADE,
    value       REAL    NOT NULL,
    achieved_at INTEGER NOT NULL,
    PRIMARY KEY (mode, param, metric)
);

CREATE TABLE keystroke_blob (          -- optional full log for replay; off by default
    session_id INTEGER PRIMARY KEY REFERENCES session(id) ON DELETE CASCADE,
    encoding   TEXT NOT NULL,
    data       BLOB NOT NULL
);
