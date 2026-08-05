// This must NOT compile with typeit::tui's include directories. If it ever
// does, the driving adapter has grown a dependency on a driven one and the
// composition root has stopped being the only place both are visible.
#include "typeit/infra/db/SqliteDatabase.h"

int uses_infra() { return typeit::infra::SqliteDatabase::open_in_memory() ? 1 : 0; }
