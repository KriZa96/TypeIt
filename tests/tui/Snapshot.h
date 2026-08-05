// Rendering, compared against a file somebody can read (TI-095, TESTING §6).
//
// A golden is plain text in `tests/tui/goldens/`. It shows up in a diff as the
// screen it is, so a rendering change is reviewed by looking at it rather than
// by trusting a hash — and a change nobody meant is obvious rather than a
// number that moved.
//
// Updating one is deliberate: `TYPEIT_UPDATE_GOLDENS=1 ctest` rewrites them,
// and CI never sets it. A harness that regenerated on mismatch would make
// every snapshot test vacuous.
#ifndef TYPEIT_TESTING_SNAPSHOT_H
#define TYPEIT_TESTING_SNAPSHOT_H

#include <cstddef>
#include <ftxui/component/component_base.hpp>
#include <ftxui/dom/node.hpp>
#include <string>
#include <string_view>

namespace typeit::testing {

    /// A component drawn to a fixed screen, as text with newlines.
    [[nodiscard]] std::string render_to_text(const ftxui::Component& component, std::size_t columns, std::size_t rows);

    /// The same for a bare element, which is what a widget that is a function
    /// rather than a component returns.
    [[nodiscard]] std::string render_to_text(ftxui::Element element, std::size_t columns, std::size_t rows);

    /// The same, styling included. For the tests that are *about* colour —
    /// a golden with escape sequences in it is reviewable by nobody, so the
    /// two questions get two functions.
    [[nodiscard]] std::string render_to_styled_text(const ftxui::Component& component, std::size_t columns,
                                                    std::size_t rows);

    /// Renders twice and fails if the two differ.
    ///
    /// The render half of the purity guard, available to every widget test.
    /// The *model* half cannot live here — this does not know what state the
    /// widget draws from — so a widget that owns one asserts on it directly,
    /// as `TypingAreaTest` does.
    void expect_render_is_pure(const ftxui::Component& component, std::size_t columns, std::size_t rows);

    /// Compares against `tests/tui/goldens/<name>.txt`, failing with both
    /// screens printed side by side. Writes the file instead when
    /// `TYPEIT_UPDATE_GOLDENS` is set.
    void expect_matches_golden(std::string_view name, const std::string& rendered);

}  // namespace typeit::testing

#endif  // TYPEIT_TESTING_SNAPSHOT_H
