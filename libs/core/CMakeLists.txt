# typeit::core — the domain, and nothing else (ADR-001).
#
# The absence of target_link_libraries below is the enforcement mechanism, not
# an omission: FTXUI's include directories are never visible here, so
# `#include <ftxui/...>` inside core is a compile error rather than a review
# comment. tests/core asserts exactly that.
add_library(typeit_core STATIC src/config/Validation.cpp src/metrics/ErrorMap.cpp src/race/DifficultyController.cpp src/race/Pacer.cpp src/modes/ModeRegistry.cpp src/modes/QuoteMode.cpp src/modes/TimedMode.cpp src/modes/WordCountMode.cpp src/modes/ZenMode.cpp src/metrics/KeyStats.cpp src/metrics/Metrics.cpp src/metrics/RollingWpm.cpp src/metrics/Timeline.cpp src/session/KeystrokeLog.cpp src/session/Session.cpp src/session/TypingModel.cpp src/session/TypingRules.cpp src/text_supply/ChunkedProvider.cpp src/text_supply/Prng.cpp src/text_supply/ShuffledSentenceProvider.cpp src/text_supply/WholeTextProvider.cpp src/text_supply/WordPoolProvider.cpp src/text/Difficulty.cpp src/text/Segmenter.cpp src/text/TextBuffer.cpp src/text/TextNormalizer.cpp src/text/Width.cpp src/text/Wrapper.cpp src/text/Utf8.cpp src/util/Result.cpp src/util/Sha256.cpp)
add_library(typeit::core ALIAS typeit_core)

target_include_directories(typeit_core PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>")

# core is the one target that needs C++23 today: std::expected arrives with
# TI-025. The floor in docs/BUILD.md section 2 is chosen to support it.
target_compile_features(typeit_core PUBLIC cxx_std_23)

typeit_set_warnings(typeit_core)
typeit_enable_coverage(typeit_core)
typeit_enable_sanitizers(typeit_core)
typeit_enable_static_analysis(typeit_core)
