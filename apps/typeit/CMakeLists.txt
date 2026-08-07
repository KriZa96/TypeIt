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

    add_test(NAME cli.end_to_end
             COMMAND "${CMAKE_COMMAND}" "-DTYPEIT=$<TARGET_FILE:typeit_binary>"
                     "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/e2e-work" -P
                     "${CMAKE_CURRENT_SOURCE_DIR}/e2e.cmake")
endif()
