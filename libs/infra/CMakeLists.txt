# typeit::infra — the driven adapters (ADR-001).
#
# SQLite and toml++ are linked PRIVATE and are not part of this library's
# interface. That is the whole point: `core` must not be able to see them, and
# neither must `app` or `tui` through their dependency on this. tests/infra
# asserts it with a probe that must fail to compile.
add_library(typeit_infra STATIC src/Dependencies.cpp)
add_library(typeit::infra ALIAS typeit_infra)

target_include_directories(typeit_infra PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>")

target_link_libraries(typeit_infra PUBLIC typeit::core)
target_link_libraries(typeit_infra PRIVATE SQLite::SQLite3 tomlplusplus::tomlplusplus)

target_compile_features(typeit_infra PUBLIC cxx_std_23)

typeit_set_warnings(typeit_infra)
typeit_enable_coverage(typeit_infra)
typeit_enable_sanitizers(typeit_infra)
typeit_enable_static_analysis(typeit_infra)
