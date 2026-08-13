# The relocation test (TI-137, and the last word on defect C2).
#
# 1.0 found its corpora with `__FILE__` evaluated at runtime, so a binary only
# worked on the machine that compiled it with the sources still in place. The
# replacement resolves them from the executable's own directory. This proves it
# the only way that counts: install to a prefix, **move the prefix somewhere
# else**, and run the binary from there.
#
# Moving matters. Installing and running in place would also pass with the
# configured install prefix baked in, which is a different mechanism and not the
# one that makes a package relocatable. After the move, the baked prefix points
# at a directory that no longer exists, and only the executable-relative lookup
# can succeed.
#
# The environment is scrubbed for the same reason. `TYPEIT_ASSETS_DIR` and
# `XDG_DATA_DIRS` both come earlier in the search order, and `./assets` comes
# later — so the working directory is a scratch one rather than the source tree,
# or the test would pass by finding the assets it was trying to do without.
#
# Run as:
#   cmake -DBUILD=<build dir> -DWORK=<scratch dir> -P relocate.cmake

if(NOT BUILD OR NOT WORK)
    message(FATAL_ERROR "relocate.cmake needs -DBUILD=<build dir> and -DWORK=<scratch dir>")
endif()

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/elsewhere")

execute_process(
    COMMAND "${CMAKE_COMMAND}" --install "${BUILD}" --prefix "${WORK}/prefix"
    OUTPUT_VARIABLE install_out
    ERROR_VARIABLE install_err
    RESULT_VARIABLE install_status)
if(NOT install_status EQUAL 0)
    message(FATAL_ERROR "install failed (${install_status}):\n${install_out}\n${install_err}")
endif()

# Nothing but ours. A dependency fetched into this build contributes its own
# install rules, and a prefix containing FTXUI's headers is a package somebody
# has to explain (TI-138 checks the manifest; this catches the common case).
file(GLOB_RECURSE installed RELATIVE "${WORK}/prefix" "${WORK}/prefix/*")
foreach(entry IN LISTS installed)
    if(NOT entry MATCHES "^(bin/typeit|share/typeit/)")
        message(FATAL_ERROR "install put something unexpected in the prefix: ${entry}")
    endif()
endforeach()

# The move. From here on the prefix the binary was configured with does not
# exist, and neither does anything else it could fall back to.
file(RENAME "${WORK}/prefix" "${WORK}/moved")

if(WIN32)
    set(binary "${WORK}/moved/bin/typeit.exe")
else()
    set(binary "${WORK}/moved/bin/typeit")
endif()
if(NOT EXISTS "${binary}")
    message(FATAL_ERROR "no binary at ${binary} after the move")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env --unset=TYPEIT_ASSETS_DIR --unset=XDG_DATA_DIRS
            "XDG_CONFIG_HOME=${WORK}/home/config" "XDG_DATA_HOME=${WORK}/home/data"
            "${binary}" --doctor
    WORKING_DIRECTORY "${WORK}/elsewhere"
    OUTPUT_VARIABLE doctor_out
    ERROR_VARIABLE doctor_err
    RESULT_VARIABLE doctor_status)
if(NOT doctor_status EQUAL 0)
    message(FATAL_ERROR "the relocated binary did not run (${doctor_status}):\n${doctor_out}\n${doctor_err}")
endif()

# Found, and found *there* — under the directory the tree was moved to, rather
# than anywhere it might have been left behind.
string(FIND "${doctor_out}" "assets: ${WORK}/moved/share/typeit" at)
if(at EQUAL -1)
    message(FATAL_ERROR
            "the relocated binary did not resolve its assets from its own path:\n${doctor_out}\n${doctor_err}")
endif()

# And the texts really are readable through it, not merely present.
foreach(corpus simple medium hard)
    if(NOT EXISTS "${WORK}/moved/share/typeit/texts/${corpus}.txt")
        message(FATAL_ERROR "the ${corpus} corpus did not survive the install")
    endif()
endforeach()

message(STATUS "relocated to ${WORK}/moved and found its own assets")
