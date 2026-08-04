# Line coverage instrumentation, behind TYPEIT_COVERAGE, used by the
# linux-coverage preset and the coverage workflow.
#
# gcc/gcov rather than llvm-cov: gcovr reads both, and the CI image already has
# gcc. Optimisation is off in the Debug build this inherits from, which matters
# — inlining moves lines around and makes a report that disagrees with the
# source people are reading.
#
# Coverage is a diagnostic, not a target (TESTING section 9). The gates exist
# because uncovered code in `core` has no excuse: there is nothing to stand up,
# no I/O to fake, and no dependency to blame.

option(TYPEIT_COVERAGE "Build with coverage instrumentation" OFF)

function(typeit_enable_coverage target)
    if(NOT TYPEIT_COVERAGE)
        return()
    endif()

    if(MSVC)
        message(WARNING "TYPEIT_COVERAGE does nothing with MSVC; use the linux-coverage preset")
        return()
    endif()

    target_compile_options(${target} PRIVATE --coverage -fprofile-update=atomic)
    target_link_options(${target} PRIVATE --coverage)
endfunction()
