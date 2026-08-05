# Which layer may include which library, checked by reading the sources.
#
# Run as `cmake -P`, registered as a test by tests/infra.
#
# The compile probes in tests/core are the stronger check where they work: they
# prove the header is *unreachable*, not merely unused. But that only holds when
# the library arrives through FetchContent with private include directories. A
# distro SQLite or toml++ lives in /usr/include, where the compiler finds it
# from anywhere on the machine, and no `PRIVATE` in any CMakeLists can change
# that — so on most Linux boxes a probe expecting `#include <sqlite3.h>` to fail
# would itself fail, and a test that is red on a normal developer setup is a
# test nobody keeps.
#
# This check is machine-independent: it does not ask whether a header could be
# found, it asks whether anyone reached for it. That is the rule ADR-001
# actually states.

set(TYPEIT_FORBIDDEN_IN_CORE "sqlite3\\.h" "toml\\+\\+/" "ftxui/" "typeit/infra/" "typeit/app/")
set(TYPEIT_FORBIDDEN_IN_APP "sqlite3\\.h" "toml\\+\\+/" "ftxui/" "typeit/infra/")
set(TYPEIT_FORBIDDEN_IN_CLI "sqlite3\\.h" "toml\\+\\+/" "ftxui/" "typeit/infra/")
set(TYPEIT_FORBIDDEN_IN_TUI "sqlite3\\.h" "toml\\+\\+/" "typeit/infra/")
set(TYPEIT_FORBIDDEN_IN_INFRA "ftxui/")

function(typeit_check_layer layer directory)
    if(NOT IS_DIRECTORY "${directory}")
        return()
    endif()

    file(GLOB_RECURSE sources "${directory}/*.h" "${directory}/*.hpp" "${directory}/*.cpp")
    foreach(source IN LISTS sources)
        file(READ "${source}" text)
        foreach(pattern IN LISTS TYPEIT_FORBIDDEN_IN_${layer})
            if(text MATCHES "#[ \t]*include[ \t]*[<\"][^>\"]*${pattern}")
                message(SEND_ERROR "${source} includes a library ${layer} may not depend on (matched '${pattern}')")
            endif()
        endforeach()
    endforeach()
endfunction()

# Defect C2: 1.0 locates its bundled texts with `__FILE__` evaluated at
# runtime, so the binary only works on the machine that compiled it. The
# replacement is AssetLocator (TI-055); this makes sure the habit does not come
# back. The legacy tree under include/ still has the original and is exempt
# until it is deleted whole at TI-097.
function(typeit_check_no_source_paths directory)
    if(NOT IS_DIRECTORY "${directory}")
        return()
    endif()
    file(GLOB_RECURSE sources "${directory}/*.h" "${directory}/*.hpp" "${directory}/*.cpp")
    foreach(source IN LISTS sources)
        file(STRINGS "${source}" lines REGEX "__FILE__")
        foreach(line IN LISTS lines)
            # A comment may name the macro — the header that replaces it says
            # what it replaces, and should. Only code counts.
            string(REGEX REPLACE "^[ \t]+" "" trimmed "${line}")
            if(NOT trimmed MATCHES "^(//|\\*|/\\*)")
                message(SEND_ERROR
                        "${source} uses __FILE__; a build-time path is not where an installed binary lives (C2)")
            endif()
        endforeach()
    endforeach()
endfunction()

typeit_check_no_source_paths("${TYPEIT_SOURCE_DIR}/libs")
typeit_check_no_source_paths("${TYPEIT_SOURCE_DIR}/apps")

# TECHNICAL section 4.2: no string-concatenated SQL anywhere, every variable a
# bound parameter. The rule is not really about SQL injection here — this is a
# local database with no untrusted input — it is that a query built by
# concatenation cannot be cached by its text, and that a value formatted into
# SQL is a value formatted by whatever locale and rounding the formatter felt
# like.
#
# The check is deliberately blunt, and reads one line at a time: a string
# literal containing a SQL keyword, on a line that also concatenates. A query
# whose `+` lands on the *next* line slips past it — which happened once, in
# this repository, and was fixed by writing the query out rather than by making
# the check cleverer. The rule is absolute; the check is a reminder, not the
# authority.
function(typeit_check_no_concatenated_sql directory)
    if(NOT IS_DIRECTORY "${directory}")
        return()
    endif()
    file(GLOB_RECURSE sources "${directory}/*.h" "${directory}/*.hpp" "${directory}/*.cpp")
    foreach(source IN LISTS sources)
        file(STRINGS "${source}" lines
             REGEX "\"[^\"]*(SELECT|INSERT|UPDATE|DELETE FROM|CREATE TABLE|DROP TABLE|PRAGMA)[^\"]*\"")
        foreach(line IN LISTS lines)
            string(REGEX REPLACE "^[ \t]+" "" trimmed "${line}")
            if(trimmed MATCHES "^(//|\\*|/\\*)")
                continue()
            endif()
            # `"… " +`, `+ "…"`, `.append(`, or a format call: all the ways a
            # value gets glued into a statement.
            if(line MATCHES "\"[^\"]*\"[ \t]*\\+" OR line MATCHES "\\+[ \t]*\"" OR line MATCHES "std::format|\\.append\\(|<<")
                message(SEND_ERROR "${source}: SQL built by concatenation — bind a parameter instead:\n    ${trimmed}")
            endif()
        endforeach()
    endforeach()
endfunction()

typeit_check_no_concatenated_sql("${TYPEIT_SOURCE_DIR}/libs")
typeit_check_no_concatenated_sql("${TYPEIT_SOURCE_DIR}/apps")

typeit_check_layer(CORE "${TYPEIT_SOURCE_DIR}/libs/core")
typeit_check_layer(APP "${TYPEIT_SOURCE_DIR}/libs/app")
typeit_check_layer(CLI "${TYPEIT_SOURCE_DIR}/libs/cli")
typeit_check_layer(TUI "${TYPEIT_SOURCE_DIR}/libs/tui")
typeit_check_layer(INFRA "${TYPEIT_SOURCE_DIR}/libs/infra")

message(STATUS "Layer isolation: every layer includes only what it may")
