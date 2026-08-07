-- A covering index for the per-day aggregation the history screen opens with
-- (TI-109).
--
-- `idx_session_started` covers the ordering but not the columns: the daily
-- totals read `net_wpm`, `accuracy` and `duration_ms`, so SQLite fell back to
-- reading the whole `session` row — twenty-three columns including two of free
-- text — for every run in the history. `EXPLAIN QUERY PLAN` said `SCAN
-- session`, and the test that reads it is what caught this.
--
-- The column order is the one the query needs: `started_at` first because the
-- day expression and the date filters are computed from it, then the two
-- columns the WHERE clause narrows on, then the three the aggregates sum. With
-- all six present SQLite can answer from the index alone and never touch the
-- row payload.
--
-- Wide for an index, and worth it: the alternative is that a history screen
-- gets slower in proportion to how much somebody has typed, which is exactly
-- the wrong way round.
CREATE INDEX idx_session_daily
    ON session(started_at, mode, completed, net_wpm, accuracy, duration_ms);
