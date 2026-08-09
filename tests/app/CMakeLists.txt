# The app layer's tests. The layer rule — app sees core and never infra — is
# checked by infra.lint.source_rules, which reads the sources rather than
# relying on an include path that a system package would defeat anyway.
add_executable(app_tests BookmarkTest.cpp CodeExtractorTest.cpp ConfigServiceTest.cpp DirectoryImportTest.cpp ExtractorRegistryTest.cpp MarkdownExtractorTest.cpp MimeDetectionTest.cpp HistoryServiceTest.cpp ImportFileTest.cpp PortsTest.cpp ProfileServiceTest.cpp SessionServiceTest.cpp SubtitleExtractorTest.cpp TextLibraryServiceTest.cpp TextSectionTest.cpp TypingReadinessTest.cpp)

# typeit_version carries the generated header: the run record's app_version is
# read from it, and the test asserts that rather than a literal.
target_link_libraries(app_tests PRIVATE typeit::app typeit::test_support typeit_version
                                        GTest::gtest GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(app_tests DISCOVERY_TIMEOUT 60)

typeit_set_warnings(app_tests)
typeit_enable_coverage(app_tests)
typeit_enable_sanitizers(app_tests)
typeit_enable_static_analysis(app_tests)
