# typeit::app — use cases and the ports the adapters implement (ADR-001).
#
# Links core and nothing else. `infra` implements these interfaces and depends
# on this library; the arrow points that way and never back, which is what
# makes a service testable with a fake and a database testable without one.
add_library(typeit_app STATIC src/ingest/ExtractorRegistry.cpp src/ingest/Ingestion.cpp src/ingest/Markdown.cpp src/ingest/MimeDetection.cpp src/ingest/PlainText.cpp src/Capabilities.cpp src/Json.cpp src/Ports.cpp src/Theme.cpp src/services/ConfigService.cpp src/services/HistoryService.cpp src/services/ProfileService.cpp src/services/SessionService.cpp src/services/TextLibraryService.cpp)
add_library(typeit::app ALIAS typeit_app)

target_include_directories(typeit_app PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>")

# typeit_version carries the generated header, privately: `app_version` is
# recorded off `kVersionString` and never spelled out (VERSIONING section 6),
# and nothing in this library's public headers needs to see it.
target_link_libraries(typeit_app PUBLIC typeit::core PRIVATE typeit_version)

target_compile_features(typeit_app PUBLIC cxx_std_23)

typeit_set_warnings(typeit_app)
typeit_enable_coverage(typeit_app)
typeit_enable_sanitizers(typeit_app)
typeit_enable_static_analysis(typeit_app)
