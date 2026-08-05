# The rules that are only worth stating if something enforces them: SQLite and
# toml++ end at infra's edge (ADR-001, ADR-013), nothing outside the legacy
# tree locates itself with __FILE__ (defect C2), and no SQL is built by
# concatenation (TECHNICAL section 4.2).
#
# Not a compile probe, unlike tests/core's FTXUI one. A probe proves the header
# is *unreachable*, which holds only while the library comes from FetchContent
# with private include directories; a distro SQLite lives in /usr/include,
# where the compiler finds it from anywhere on the machine whatever any
# CMakeLists says. A probe expecting the include to fail would itself fail on
# most Linux boxes, and a test that is red on a normal developer setup is a test
# nobody keeps.
#
# So the check reads the sources instead: not "could this header be found" but
# "did anyone reach for it", which is the rule ADR-001 actually states. It
# covers FTXUI in core and app too, where it is machine-independent in a way the
# probe is not.
add_test(NAME infra.lint.source_rules
         COMMAND "${CMAKE_COMMAND}" "-DTYPEIT_SOURCE_DIR=${CMAKE_SOURCE_DIR}" -P
                 "${CMAKE_SOURCE_DIR}/cmake/CheckLayerIsolation.cmake")

add_executable(infra_tests AssetLocatorTest.cpp ConfigMigrationTest.cpp DependenciesTest.cpp DoctorTest.cpp MigratorTest.cpp PlatformPathsTest.cpp SqliteDatabaseTest.cpp SqliteHistoryRepositoryTest.cpp SqliteTextLibraryRepositoryTest.cpp StdFileSystemTest.cpp TempEnvTest.cpp ThemeLoaderTest.cpp TomlConfigStoreTest.cpp)

# typeit_version carries the generated header: a test that needs the
# application version reads it rather than spelling one (VERSIONING section 6).
target_link_libraries(infra_tests PRIVATE typeit::infra typeit::test_support typeit_version
                                         GTest::gtest GTest::gtest_main)

# The shipped themes are read from the source tree, never through __FILE__
# (defect C2): a test of the files must fail on the machine that edits them,
# and the path is compiled in rather than guessed at runtime.
target_compile_definitions(infra_tests PRIVATE TYPEIT_ASSETS_DIR="${CMAKE_SOURCE_DIR}/assets")

include(GoogleTest)
gtest_discover_tests(infra_tests DISCOVERY_TIMEOUT 60)

typeit_set_warnings(infra_tests)
typeit_enable_coverage(infra_tests)
typeit_enable_sanitizers(infra_tests)
typeit_enable_static_analysis(infra_tests)
