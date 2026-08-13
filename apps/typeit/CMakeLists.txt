# The composition root, and the only target that links every layer.
#
# Named `typeit_app_binary` internally because `typeit_app` is the use-case
# library; the file it produces is plain `typeit`, which is what a user types.
add_executable(typeit_binary main.cpp)
set_target_properties(typeit_binary PROPERTIES OUTPUT_NAME typeit)

target_link_libraries(typeit_binary PRIVATE typeit::cli typeit::tui typeit::infra typeit::app typeit::core)

typeit_set_warnings(typeit_binary)
typeit_enable_coverage(typeit_binary)
typeit_enable_sanitizers(typeit_binary)
typeit_enable_static_analysis(typeit_binary)

# Install rules (TI-137, BUILD section 8).
#
# The binary and the assets it looks for, in the layout `AssetLocator` searches:
# `<prefix>/bin/typeit` beside `<prefix>/share/typeit`. That relative pair is
# what makes an install tree *relocatable* — move it anywhere and the binary
# still finds its texts from its own path, which is the direct fix for defect
# C2, where 1.0 located assets with `__FILE__` and so only worked on the machine
# that compiled it.
#
# No man page yet: `docs/typeit.1` arrives with TI-139, and installing a file
# that does not exist fails the install rather than skipping it.
include(GNUInstallDirs)

install(TARGETS typeit_binary RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
install(DIRECTORY "${CMAKE_SOURCE_DIR}/assets/texts/"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/typeit/texts")
install(DIRECTORY "${CMAKE_SOURCE_DIR}/assets/themes/"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/typeit/themes")

if(TYPEIT_BUILD_TESTS)
    # Phase 3's exit criterion, and the smoke tests around it. These run the
    # real binary: everything else in the suite links a library, and a program
    # that links perfectly can still fail to start.
    add_test(NAME cli.version COMMAND typeit_binary --version)
    set_tests_properties(cli.version PROPERTIES PASS_REGULAR_EXPRESSION "^typeit ${PROJECT_VERSION}")

    add_test(NAME cli.help COMMAND typeit_binary --help)
    set_tests_properties(cli.help PROPERTIES PASS_REGULAR_EXPRESSION "--simulate")

    # Pointed at a scratch directory, not the developer's own: `--doctor`
    # proves a directory is writable by writing in it, and no test may touch a
    # real config or history.
    add_test(NAME cli.doctor COMMAND typeit_binary --doctor)
    set_tests_properties(
        cli.doctor
        PROPERTIES PASS_REGULAR_EXPRESSION "\\[database\\]" ENVIRONMENT
                   "TYPEIT_CONFIG_DIR=${CMAKE_CURRENT_BINARY_DIR}/doctor-work/config;TYPEIT_DATA_DIR=${CMAKE_CURRENT_BINARY_DIR}/doctor-work/data")

    # A misused command line exits non-zero, or a script that checks the status
    # learns nothing.
    add_test(NAME cli.unknown_flag_is_refused COMMAND typeit_binary --nonsense)
    set_tests_properties(cli.unknown_flag_is_refused PROPERTIES WILL_FAIL TRUE)

    # The text library, over the real binary and a scratch database. What these
    # add over `TextCliTest` is the exit status, which is the half a script
    # reads and the half a unit test cannot see.
    set(text_work "${CMAKE_CURRENT_BINARY_DIR}/text-work")
    add_test(NAME cli.texts_empty_library
             COMMAND typeit_binary --data-dir "${text_work}/empty" --list-texts)
    set_tests_properties(cli.texts_empty_library PROPERTIES PASS_REGULAR_EXPRESSION "No texts yet")

    add_test(NAME cli.texts_remove_needs_confirmation
             COMMAND typeit_binary --data-dir "${text_work}/empty" --remove-text 1)
    set_tests_properties(cli.texts_remove_needs_confirmation PROPERTIES WILL_FAIL TRUE)

    add_test(NAME cli.texts_unknown_id_is_refused
             COMMAND typeit_binary --data-dir "${text_work}/empty" --text-id 999 --simulate /nonexistent)
    set_tests_properties(cli.texts_unknown_id_is_refused PROPERTIES WILL_FAIL TRUE)

    # Defect C2, proved the only way that counts: install, move the tree, run
    # it from there (TI-137). Serialised against the other build-driving tests
    # because it runs `cmake --install` over this build directory.
    add_test(NAME cli.relocatable
             COMMAND "${CMAKE_COMMAND}" "-DBUILD=${CMAKE_BINARY_DIR}"
                     "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/relocate-work" -P
                     "${CMAKE_CURRENT_SOURCE_DIR}/relocate.cmake")
    set_tests_properties(cli.relocatable PROPERTIES RESOURCE_LOCK build_directory)

    add_test(NAME cli.end_to_end
             COMMAND "${CMAKE_COMMAND}" "-DTYPEIT=$<TARGET_FILE:typeit_binary>"
                     "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/e2e-work" -P
                     "${CMAKE_CURRENT_SOURCE_DIR}/e2e.cmake")
endif()
