# ASan and UBSan, behind TYPEIT_SANITIZERS, used by the linux-clang-asan preset.
#
# -fno-sanitize-recover=all matters more than the sanitizers themselves: without
# it a finding prints and the process exits zero, and a green CI run means
# nothing.
#
# TSan and MSan are deliberately absent. There is no threading model worth
# checking until Phase 4, and one thread that posts events is not it; CI-013
# runs TSan nightly once there is something to find.

option(TYPEIT_SANITIZERS "Build with AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

function(typeit_enable_sanitizers target)
    if(NOT TYPEIT_SANITIZERS)
        return()
    endif()

    if(MSVC)
        # MSVC has ASan but no UBSan, and the two flag spellings share nothing.
        target_compile_options(${target} PRIVATE /fsanitize=address)
        return()
    endif()

    set(flags -fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all)
    target_compile_options(${target} PRIVATE ${flags})
    target_link_options(${target} PRIVATE ${flags})

    # Hardened standard library assertions: an out-of-range operator[] becomes a
    # clean abort instead of a read past the end.
    target_compile_definitions(${target} PRIVATE _GLIBCXX_ASSERTIONS _LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_FAST)
endfunction()
