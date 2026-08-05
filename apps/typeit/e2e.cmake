# Phase 3's exit criterion, run against the real binary.
#
# `typeit --simulate script.tks` drives a complete session through the real
# stack — real services, real modes, real SQLite in a temp directory — with no
# terminal, and writes a correct row. Then `--export` reads that row back.
#
# Written as a CMake script rather than a shell one because it has to run on
# Windows too, and because `execute_process` gives the exit code, stdout and
# stderr of each step without a shell in between.
#
# Run as:
#   cmake -DTYPEIT=<binary> -DWORK=<scratch dir> -P e2e.cmake

if(NOT TYPEIT OR NOT WORK)
    message(FATAL_ERROR "e2e.cmake needs -DTYPEIT=<binary> and -DWORK=<dir>")
endif()

function(expect_contains what haystack needle)
    string(FIND "${haystack}" "${needle}" at)
    if(at EQUAL -1)
        message(FATAL_ERROR "${what}: expected to find '${needle}' in:\n${haystack}")
    endif()
endfunction()

function(run_typeit)
    cmake_parse_arguments(STEP "" "WHAT" "ARGS" ${ARGN})
    execute_process(
        COMMAND "${TYPEIT}" ${STEP_ARGS}
        OUTPUT_VARIABLE out
        ERROR_VARIABLE err
        RESULT_VARIABLE code)
    if(NOT code EQUAL 0)
        message(FATAL_ERROR "${STEP_WHAT}: exit ${code}\n${err}")
    endif()
    set(OUTPUT "${out}" PARENT_SCOPE)
endfunction()

# A directory nothing else owns, so the run cannot reach a real history.
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")

file(WRITE "${WORK}/text.txt" "hi there")
file(WRITE "${WORK}/keys.tks"
     "# typeit-script 1\n"
     "1000  type h\n"
     "1500  type i\n"
     "2000  type space\n"
     "2500  type t\n"
     "3000  type h\n"
     "3500  type e\n"
     "4000  type r\n"
     "5000  type e\n")

run_typeit(WHAT "--simulate" ARGS --simulate "${WORK}/keys.tks" --data-dir "${WORK}"
                                  --mode quote --text "${WORK}/text.txt")
expect_contains("--simulate" "${OUTPUT}" "\"schema\":1")
expect_contains("--simulate" "${OUTPUT}" "\"completed\":true")
expect_contains("--simulate" "${OUTPUT}" "\"graphemes_typed\":8")
expect_contains("--simulate" "${OUTPUT}" "\"duration_ms\":4000")

# Determinism: the same script, the same answer, with a whole session written
# to the database in between.
set(FIRST "${OUTPUT}")
run_typeit(WHAT "--simulate again" ARGS --simulate "${WORK}/keys.tks" --data-dir "${WORK}"
                                        --mode quote --text "${WORK}/text.txt")
if(NOT FIRST STREQUAL OUTPUT)
    message(FATAL_ERROR "two runs of one script disagreed:\n${FIRST}\n---\n${OUTPUT}")
endif()

# And the row is really there, read back through the other half of the program.
run_typeit(WHAT "--export csv" ARGS --export csv --data-dir "${WORK}")
expect_contains("--export csv" "${OUTPUT}" "id,started_at,mode")
expect_contains("--export csv" "${OUTPUT}" "quote")

run_typeit(WHAT "--export json" ARGS --export json --data-dir "${WORK}")
expect_contains("--export json" "${OUTPUT}" "\"net_wpm\"")

run_typeit(WHAT "--stats" ARGS --stats --data-dir "${WORK}")
expect_contains("--stats" "${OUTPUT}" "sessions: 2")
expect_contains("--stats" "${OUTPUT}" "[personal bests]")

run_typeit(WHAT "--last" ARGS --export csv --last 1 --data-dir "${WORK}")
string(REGEX MATCHALL "\n" newlines "${OUTPUT}")
list(LENGTH newlines line_count)
if(NOT line_count EQUAL 2)
    message(FATAL_ERROR "--last 1 should give a header and one row, got ${line_count} lines:\n${OUTPUT}")
endif()

file(REMOVE_RECURSE "${WORK}")
message(STATUS "end to end: a session ran, was saved, and was read back")
