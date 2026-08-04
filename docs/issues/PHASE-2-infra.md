# Phase 2 — `typeit::infra`

**Milestone:** `v2.0.0-alpha.3` · **Issues:** TI-053 – TI-065 · **Goal:** persistence,
configuration, and asset location behind ports.

Everything here is an adapter. The domain does not know these classes exist. Tests use a real
in-memory SQLite rather than a mock, because the SQL is the part most likely to be wrong and
mocking it would test nothing.

The legacy application remains untouched.

---

## TI-053 — Scaffold `typeit_infra` and add SQLite + toml++

**Type** build · **Size** S · **Priority** P0 · **Depends on** TI-023, TI-007 · **Docs** [BUILD §4](../BUILD.md#4-dependencies-adr-007)

**Scope**
- In: `libs/infra` target linking `typeit::core` plus SQLite3 and toml++ **`PRIVATE`** so
  neither leaks to consumers; both added to `cmake/Dependencies.cmake` with
  `FIND_PACKAGE_ARGS`; SQLite amalgamation hash-pinned.
- Out: any adapter implementation.

**Tests**
- Negative build test: a translation unit outside `infra` including `<sqlite3.h>` or
  `<toml++/toml.h>` fails to compile.
- Dependency resolution verified on all four paths from TI-007.

**Acceptance**
- [x] `infra` links core + SQLite + toml++; nothing else, and both are `PRIVATE` so neither
      reaches a consumer's include path.
- [x] SQLite and toml++ are not reached from `core`, `app` or `tui` —
      `infra.isolation.layers_include_only_what_they_may`, watched to fail on a deliberate
      `#include <sqlite3.h>` in core and to pass when it is removed.
- [x] **Not a compile probe, unlike tests/core's FTXUI one, and the difference matters.** A
      probe proves a header is *unreachable*, which holds only while the library comes from
      FetchContent with private include directories. A distro SQLite lives in `/usr/include`,
      where the compiler finds it from anywhere on the machine whatever any `CMakeLists` says —
      the probe was written first, and it compiled, because the header is simply there. The
      check now reads the sources: not "could this be found" but "did anyone reach for it",
      which is the rule ADR-001 actually states. It covers FTXUI in `core` and `app` as well,
      where it is machine-independent in a way the existing probe is not.
- [x] Both dependency paths verified by building each: system packages (SQLite 3.53.3 here) and
      the hash-pinned download (3.53.4), the latter forced with
      `-DCMAKE_DISABLE_FIND_PACKAGE_SQLite3=ON -DCMAKE_DISABLE_FIND_PACKAGE_tomlplusplus=ON`.

---

## TI-054 — `PlatformPaths`

**Type** feat · **Size** M · **Priority** P0 · **Depends on** TI-053 · **Docs** [TECHNICAL §4.1](../TECHNICAL.md#41-platform-paths)

XDG Base Directory on Linux, `%APPDATA%`/`%LOCALAPPDATA%` on Windows, with
`TYPEIT_CONFIG_DIR` / `TYPEIT_DATA_DIR` overrides. Those overrides are what make every later
integration test hermetic — no test ever touches a developer's real history.

**Unit tests** (`PlatformPathsTest.cpp`) — environment injected, never read from the real
process where avoidable
- `XDG_CONFIG_HOME` set → used verbatim.
- `XDG_CONFIG_HOME` unset → `~/.config/typeit`.
- `XDG_CONFIG_HOME` set to a relative path → rejected per spec (must be absolute).
- Windows: `%APPDATA%\TypeIt` and `%LOCALAPPDATA%\TypeIt`.
- `TYPEIT_CONFIG_DIR` / `TYPEIT_DATA_DIR` override everything.
- Missing `HOME` (and `%APPDATA%`) → a clear `Result` error, not a crash.
- Directory creation is idempotent and reports permission failures.

**Acceptance**
- [x] Correct on Linux and Windows, verified in CI on both — the platform-specific cases are
      compiled in per platform, so the Windows job runs the Windows expectations rather than
      skipping them.
- [x] Overrides work and win outright, including over the XDG variables. Used by every test
      fixture from TI-065 on.
- [x] The environment is a parameter, not a global read: one test exercises the real process
      environment and only reads from it. A test that had to *set* a real variable would race
      every other test in the process.

---

## TI-055 — `AssetLocator`

**Type** feat · **Size** M · **Priority** P0 · **Depends on** TI-054 · **Docs** [TECHNICAL §4.1](../TECHNICAL.md#41-platform-paths), [REVIEW C2](../CODEBASE_REVIEW.md#4-correctness-defects)

**This issue kills defect C2.** Bundled corpora are currently located with
`std::filesystem::path current_file_path = __FILE__;` evaluated at runtime, so the binary only
finds its assets on the machine that compiled it, with the source tree still at the same
absolute path. The `ln -s … /usr/games/TypeIt` step in the README works by accident.

Search order: `$TYPEIT_ASSETS_DIR` → executable dir `/../share/typeit` → configured install
prefix → `$XDG_DATA_DIRS` → `./assets` (development only).

**Unit tests** (`AssetLocatorTest.cpp`)
- Each search-path entry is tried in the documented order; first hit wins.
- A missing assets directory returns a clear error and the application still functions with
  user-imported texts only.
- Executable-relative resolution works from a temp directory laid out like an install tree.
- `TYPEIT_ASSETS_DIR` overrides everything.
- Symlinked executable resolves to the real path (the `/usr/games` case).

**Acceptance**
- [ ] **Relocation test**: build, `cmake --install` to a temp prefix, move the whole tree,
      delete the source directory, run — assets are still found. This is the acceptance test
      that proves C2 is fixed.
- [ ] `git grep __FILE__` returns nothing outside diagnostics.

---

## TI-056 — `SqliteDatabase` wrapper

**Type** feat · **Size** M · **Priority** P0 · **Depends on** TI-053 · **Docs** [TECHNICAL §4.2](../TECHNICAL.md#42-sqlite)

RAII connection, prepared-statement cache, and a `Transaction` guard.

**Scope**
- In: open/close with the documented pragmas (WAL, `foreign_keys=ON`, `synchronous=NORMAL`,
  `busy_timeout=3000`); statement preparation and caching; parameter binding helpers; a
  `Transaction` type that rolls back on destruction unless committed.
- Out: schema and repositories.

**Unit tests** (`SqliteDatabaseTest.cpp`) — against `:memory:` and temp files
- Opening a non-existent path with `CREATE` succeeds; without it, fails cleanly.
- Opening a directory, or a file with no permissions, returns a `Result` error.
- Every pragma is actually applied (queried back).
- A prepared statement is reused, not re-prepared (instrumented counter).
- `Transaction` rolls back when destroyed without commit — including when unwinding.
- Nested transaction attempts are rejected or mapped to savepoints, per the documented choice.
- A statement error surfaces the SQLite message in `Error::context`.
- **No string-concatenated SQL**: a lint test greps the infra sources for SQL built by
  concatenation and fails if found.

**Acceptance**
- [ ] All writes go through `Transaction`.
- [ ] Every variable is a bound parameter; the lint test enforces it.
- [ ] Clean under ASan.

---

## TI-057 — `Migrator` and schema v1

**Type** feat · **Size** L · **Priority** P0 · **Depends on** TI-056 · **Docs** [TECHNICAL §5](../TECHNICAL.md#5-database-schema-v1), [VERSIONING §5](../VERSIONING.md#5-independent-version-numbers)

Ordered migrations keyed on `PRAGMA user_version`, each in its own transaction, with schema SQL
embedded into the binary at build time.

**Scope**
- In: the migrator; schema v1 exactly as specified in TECHNICAL §5; embedding mechanism; **the
  CI schema-version guard** deferred here from
  [CI-008](PHASE-0A-cicd.md#ci-008--version-guards-and-version-derivation) — a workflow step
  that fails when files under `libs/infra/schema/` change without a `user_version` bump and a
  migration test from the previous version.
- Out: repositories.

**Unit tests** (`MigratorTest.cpp`)
- An empty database migrates to the latest version.
- An already-current database is a no-op.
- **A database with a higher `user_version` than the binary understands is refused with a clear
  message** — not upgraded, not wiped. Explicit test.
- A failing migration rolls back completely, leaving `user_version` unchanged.
- Migrations are idempotent when re-run from a snapshot at each version.
- Schema v1 creates every table, index, and constraint in TECHNICAL §5 (introspected, not
  assumed).
- Foreign-key cascades work: deleting a `session` removes its `session_sample` rows; deleting a
  `text_item` sets `session.text_id` to NULL.
- `CHECK` constraints reject invalid `mode`, `source`, and `completed` values.

**Acceptance**
- [ ] Migration from empty and from every prior snapshot is tested.
- [ ] A future-version database is refused, with the test to prove it.
- [ ] Version numbering follows VERSIONING §5 — monotonic, never reused.
- [ ] **The CI schema guard has been watched to fail**: a scratch branch changing schema SQL
      without bumping `user_version` is blocked. First real exercise of it is
      [TX-006](PHASE-6A-text-sources.md#tx-006--schema-v2-and-section-persistence).

---

## TI-058 — `SqliteHistoryRepository`

**Type** feat · **Size** L · **Priority** P0 · **Depends on** TI-057 · **Docs** [TECHNICAL §2.1](../TECHNICAL.md#21-ports)

Implements `IHistoryRepository`: save, query, aggregates, personal bests, key-stat merge, and
`best_sustained_wpm` (which race mode's adaptive start speed depends on, and which belongs in
SQL rather than in client-side filtering).

**Unit tests** (`SqliteHistoryRepositoryTest.cpp`) — real `:memory:` database
- Save writes exactly one `session` row and N `session_sample` rows, in one transaction.
- A failure mid-save leaves the database completely unchanged.
- `query` honours mode filter, date range, limit, and ordering.
- `aggregates` computes mean/min/max/count correctly, and returns zeros (not NaN) for an empty
  range.
- Key/bigram stats merge via `ON CONFLICT DO UPDATE` — merging twice doubles counts, and a new
  grapheme inserts.
- Personal bests: a better run replaces the record; a worse run does not; an equal run does not
  (documented tie-break).
- **Abandoned runs (`completed = 0`) never set a personal best.**
- **Runs with accuracy below 90% never set a personal best.**
- `best_sustained_wpm(30 days)` respects the window boundary exactly and returns an empty
  result with no history.
- Unicode round-trips: a grapheme key such as `č` or `👍` stores and reads back intact.
- Concurrent-open behaviour under WAL is documented and tested for the busy path.

**Acceptance**
- [ ] Every port method has tests for the happy path, the empty-database path, and at least one
      error path.
- [ ] Personal-best qualification rules from [GAMEPLAY §7.3](../GAMEPLAY.md#73-personal-bests)
      are enforced in SQL and tested.

---

## TI-059 — `SqliteTextLibraryRepository`

**Type** feat · **Size** M · **Priority** P1 · **Depends on** TI-057

Implements `ITextLibraryRepository`: add, list, get, remove, tag, untag, bookmark, find by
content hash.

**Unit tests** (`SqliteTextLibraryRepositoryTest.cpp`)
- Add then get round-trips every field, including large content (1 MB) and non-ASCII.
- The `content_sha256` UNIQUE constraint rejects a duplicate, and `find_by_hash` returns the
  existing row.
- Remove cascades to tags and bookmarks.
- Bookmark upsert updates rather than duplicating.
- List supports tag filter, search, and ordering.
- Removing a text referenced by sessions sets `session.text_id` to NULL and leaves the sessions
  intact.

---

## TI-060 — `TomlConfigStore`

**Type** feat · **Size** M · **Priority** P0 · **Depends on** TI-053, TI-050 · **Docs** [TECHNICAL §6](../TECHNICAL.md#6-configuration-file)

Load, validate, apply defaults, and save the TOML configuration.

**Scope**
- In: parse → `Config`; missing keys take defaults; unknown keys warn and are ignored (forward
  compatibility); invalid values fall back to the default with a warning; write with commented
  defaults on first run.
- Out: key migration (TI-061).

**Unit tests** (`TomlConfigStoreTest.cpp`)
- A missing file yields defaults and writes a commented template.
- A complete valid file loads every field exactly.
- A partial file fills the rest from defaults.
- An unknown key produces a warning and does not fail the load.
- An out-of-range value falls back to the default **and warns**, naming the key.
- **A malformed file is reported with a line number and is never overwritten.** Explicit test —
  the user's hand-written config must survive our failure to parse it.
- Save → load round-trips identically.
- Save is atomic: a crash mid-write cannot truncate the existing file (write to temp, then
  rename).
- A read-only config directory produces a clear error.
- Non-ASCII values (theme names, paths) round-trip.

**Acceptance**
- [ ] A malformed config never destroys user data.
- [ ] Every field in TECHNICAL §6 is covered by a load test.

---

## TI-061 — Config key migration

**Type** feat · **Size** S · **Priority** P2 · **Depends on** TI-060 · **Docs** [VERSIONING §5](../VERSIONING.md#5-independent-version-numbers)

`config_version`-driven migration of renamed or re-meaninged keys, writing a backup first.

**Scope**
- In: the migration mechanism; **the CI config-version guard** deferred here from
  [CI-008](PHASE-0A-cicd.md#ci-008--version-guards-and-version-derivation) — fails when a config
  key is renamed or removed without a `config_version` bump and a migration.
- Out: —

**Unit tests** (`ConfigMigrationTest.cpp`)
- A version-1 file loads unchanged.
- A file with no `config_version` is treated as version 1.
- A future `config_version` warns and loads what it can, rather than failing outright.
- Migration writes `config.toml.bak` before modifying.
- Migration is idempotent.

**Acceptance**
- [ ] A user's settings can never be silently discarded by a rename.
- [ ] The CI guard has been watched to fail on a deliberate unbumped rename.

---

## TI-062 — `SystemClock` and `StdFileSystem`

**Type** feat · **Size** S · **Priority** P0 · **Depends on** TI-053

The two remaining trivial adapters.

**Unit tests** (`SystemClockTest.cpp`, `StdFileSystemTest.cpp`)
- `SystemClock::now()` is monotonic across successive calls.
- `read_text` on a missing file, a directory, and a permission-denied file each return distinct
  documented errors.
- `read_text` handles a zero-byte file (returns empty, does not underflow — the same class of
  bug as [C1](../CODEBASE_REVIEW.md#4-correctness-defects)).
- `list` returns entries in a deterministic order.
- Paths with non-ASCII characters work on both platforms.

---

## TI-063 — Contract test harness

**Type** test · **Size** M · **Priority** P0 · **Depends on** TI-058, TI-059 · **Docs** [TESTING §4](../TESTING.md#4-contract-tests--ports)

One typed test suite per port, run against **every** implementation, so a fake and a real
adapter cannot silently diverge — which is the failure mode that makes fakes dangerous.

**Scope**
- In: `TYPED_TEST_SUITE` harnesses for `IHistoryRepository`, `ITextLibraryRepository`,
  `IConfigStore`, `IFileSystem`; a factory abstraction per implementation.
- Out: the fakes themselves (TI-064).

**Acceptance**
- [ ] Each contract suite runs against both the SQLite and the fake implementation and passes
      identically.
- [ ] Adding a new implementation requires only a factory, not new tests.

---

## TI-064 — Fake adapters for app-layer testing

**Type** test · **Size** M · **Priority** P0 · **Depends on** TI-063

In-memory implementations of every port, used by Phase 3 service tests.

**Scope**
- In: `FakeHistoryRepository`, `FakeTextLibraryRepository`, `FakeConfigStore`,
  `FakeFileSystem`, `FakeAssetLocator`; each with injectable failure modes so error paths are
  testable.
- Out: —

**Unit tests** — the fakes are validated by the TI-063 contract suites. Additionally:
- Injected failures surface as the expected `Error`.
- Call recording (for asserting orchestration) is accurate.

**Acceptance**
- [ ] Every fake passes its port's contract suite.
- [ ] Every fake can be made to fail on demand.

---

## TI-065 — Hermetic test fixtures

**Type** test · **Size** S · **Priority** P0 · **Depends on** TI-054

A temp-directory fixture that sets `TYPEIT_CONFIG_DIR` and `TYPEIT_DATA_DIR`, and cleans up in
its destructor.

**Unit tests** (`TempEnvTest.cpp`)
- The fixture creates and removes its directory, including on test failure.
- Environment variables are restored afterwards.
- Two fixtures in the same process do not collide.

**Acceptance**
- [ ] **No test anywhere touches the real config or data directory.** Verified by a CI check
      that fails if `~/.config/typeit` or `~/.local/share/typeit` exists after a test run on a
      clean runner.

---

## Phase exit criteria

- [ ] Migrations apply from empty and from every prior version; a future version is refused.
- [ ] The relocation test passes — installed binary finds its assets with the source tree
      deleted (C2 closed).
- [ ] No SQL string concatenation; the lint test enforces it.
- [ ] A malformed config is reported and never overwritten.
- [ ] Contract suites pass identically for real and fake implementations.
- [ ] No test touches a real user directory.
- [ ] `infra` coverage ≥ 75%.
- [ ] The legacy application still builds and passes its own tests.
