# clang-tidy during compilation, behind TYPEIT_CLANG_TIDY, used by the
# linux-tidy preset. cppcheck and include-what-you-use are wired the same way
# and are off by default: they are a second opinion, not a gate.
#
# The check set lives in .clang-tidy. The legacy tree carries its own override
# (src/.clang-tidy, tests/.clang-tidy) because it is deleted at the Phase 4
# cutover, not fixed -- see TI-097.

option(TYPEIT_CLANG_TIDY "Run clang-tidy during compilation" OFF)
option(TYPEIT_CPPCHECK "Run cppcheck during compilation" OFF)
option(TYPEIT_IWYU "Run include-what-you-use during compilation" OFF)

function(typeit_enable_static_analysis target)
    if(TYPEIT_CLANG_TIDY)
        find_program(TYPEIT_CLANG_TIDY_EXE NAMES clang-tidy REQUIRED)
        set_target_properties(${target} PROPERTIES CXX_CLANG_TIDY "${TYPEIT_CLANG_TIDY_EXE}")
    endif()

    if(TYPEIT_CPPCHECK)
        find_program(TYPEIT_CPPCHECK_EXE NAMES cppcheck REQUIRED)
        set_target_properties(
            ${target} PROPERTIES CXX_CPPCHECK
                                 "${TYPEIT_CPPCHECK_EXE};--enable=warning,portability;--inline-suppr;--error-exitcode=1")
    endif()

    if(TYPEIT_IWYU)
        find_program(TYPEIT_IWYU_EXE NAMES include-what-you-use iwyu REQUIRED)
        set_target_properties(${target} PROPERTIES CXX_INCLUDE_WHAT_YOU_USE "${TYPEIT_IWYU_EXE}")
    endif()
endfunction()
