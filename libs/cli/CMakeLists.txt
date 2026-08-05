# typeit::cli — the command line, and nothing that acts on it (ADR-001).
#
# Links app, because CliOptions is what the composition root hands to the
# services. It never links infra or FTXUI: parsing a flag is not a reason to
# know what a database or a terminal is, and the layer check enforces it.
add_library(typeit_cli STATIC src/Cli.cpp src/Script.cpp src/Simulate.cpp)
add_library(typeit::cli ALIAS typeit_cli)

target_include_directories(typeit_cli PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>")

target_link_libraries(typeit_cli PUBLIC typeit::app)

target_compile_features(typeit_cli PUBLIC cxx_std_23)

typeit_set_warnings(typeit_cli)
typeit_enable_coverage(typeit_cli)
typeit_enable_sanitizers(typeit_cli)
typeit_enable_static_analysis(typeit_cli)
