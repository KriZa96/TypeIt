# Warning set from docs/BUILD.md section 6, applied per target.
#
# `-Werror` is opt-in through TYPEIT_WERROR so a local build stays workable
# while the baseline is being cleared; CI turns it on (TI-020).

option(TYPEIT_WERROR "Treat compiler warnings as errors" OFF)

function(typeit_set_warnings target)
    set(gcc_clang
        -Wall
        -Wextra
        -Wpedantic
        -Wshadow
        -Wnon-virtual-dtor
        -Wold-style-cast
        -Wcast-align
        -Wunused
        -Woverloaded-virtual
        -Wconversion
        -Wsign-conversion
        -Wnull-dereference
        -Wdouble-promotion
        -Wimplicit-fallthrough
        -Wmisleading-indentation)

    set(gcc_only -Wduplicated-cond -Wduplicated-branches -Wlogical-op -Wuseless-cast)

    # /utf-8 is required, not cosmetic: the sources contain UTF-8 string
    # literals and MSVC otherwise reads them in the system code page.
    set(msvc /W4 /permissive- /w14640 /w14826 /w14928 /utf-8)

    if(MSVC)
        target_compile_options(${target} PRIVATE ${msvc})
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_compile_options(${target} PRIVATE ${gcc_clang} ${gcc_only})
    else()
        target_compile_options(${target} PRIVATE ${gcc_clang})
    endif()

    if(TYPEIT_WERROR)
        target_compile_options(${target} PRIVATE $<IF:$<CXX_COMPILER_ID:MSVC>,/WX,-Werror>)
    endif()
endfunction()
