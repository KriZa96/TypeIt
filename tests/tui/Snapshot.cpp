#include "Snapshot.h"

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <ftxui/component/component_base.hpp>
#include <ftxui/dom/node.hpp>
#include <ftxui/screen/screen.hpp>
#include <gtest/gtest.h>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

#include "typeit/infra/fs/PlatformPaths.h"

namespace typeit::testing {
    namespace {

        std::filesystem::path golden_path(std::string_view name) {
            return std::filesystem::path{TYPEIT_GOLDEN_DIR} / (std::string{name} + ".txt");
        }

        /// Read through the environment port rather than `std::getenv`,
        /// which the Windows CRT deprecates and `/W4 -Werror` means. `infra`
        /// already owns the portable spelling; a second one here would be a
        /// second thing to get wrong.
        bool updating() {
            static const infra::Environment environment = infra::system_environment();
            return environment("TYPEIT_UPDATE_GOLDENS").has_value();
        }

        /// The screen without its styling.
        ///
        /// A golden full of `\x1b[38;2;205;214;244m` is technically plain text
        /// and reviewable by nobody. What a snapshot is for is layout — where
        /// the words are, where they wrap, what the caret sits on — and colour
        /// has its own tests: the quantiser's distinguishability property, and
        /// a widget test that asserts the four states render differently.
        std::string without_styling(std::string_view screen) {
            std::string out;
            out.reserve(screen.size());

            for (std::size_t at = 0; at < screen.size(); ++at) {
                if (screen[at] == '\r') {
                    continue;  // FTXUI ends lines with CRLF; a file need not.
                }
                if (screen[at] != '\x1b') {
                    out += screen[at];
                    continue;
                }
                // A CSI sequence: ESC [ parameters final-byte. Skipping to the
                // final byte is enough — nothing here emits anything else.
                while (at < screen.size() && (screen[at] < '@' || screen[at] > '~' || screen[at] == '[')) {
                    ++at;
                }
            }
            return out;
        }

    }  // namespace

    std::string render_to_text(ftxui::Element element, std::size_t columns, std::size_t rows) {
        ftxui::Screen screen = ftxui::Screen::Create(ftxui::Dimension::Fixed(static_cast<int>(columns)),
                                                     ftxui::Dimension::Fixed(static_cast<int>(rows)));
        ftxui::Render(screen, element);
        return without_styling(screen.ToString());
    }

    std::string render_to_text(const ftxui::Component& component, std::size_t columns, std::size_t rows) {
        return render_to_text(component->Render(), columns, rows);
    }

    std::string render_to_styled_text(const ftxui::Component& component, std::size_t columns, std::size_t rows) {
        ftxui::Screen screen = ftxui::Screen::Create(ftxui::Dimension::Fixed(static_cast<int>(columns)),
                                                     ftxui::Dimension::Fixed(static_cast<int>(rows)));
        ftxui::Render(screen, component->Render());
        return screen.ToString();
    }

    void expect_render_is_pure(const ftxui::Component& component, std::size_t columns, std::size_t rows) {
        const std::string first = render_to_styled_text(component, columns, rows);
        const std::string second = render_to_styled_text(component, columns, rows);
        EXPECT_EQ(first, second) << "rendering changed something it drew from";
    }

    void expect_render_is_pure(const std::function<ftxui::Element()>& draw, std::size_t columns, std::size_t rows) {
        // Three, not two. Twice catches a counter that ticks on every draw;
        // a third catches one that settles after the first — which is what
        // 1.0's "advance on render" looked like from the outside.
        const std::string first = render_to_text(draw(), columns, rows);
        const std::string second = render_to_text(draw(), columns, rows);
        const std::string third = render_to_text(draw(), columns, rows);
        EXPECT_EQ(first, second) << "rendering changed something it drew from";
        EXPECT_EQ(second, third) << "rendering changed something it drew from";
    }

    void expect_matches_golden(std::string_view name, const std::string& rendered) {
        const std::filesystem::path path = golden_path(name);

        if (updating()) {
            std::filesystem::create_directories(path.parent_path());
            std::ofstream file{path, std::ios::binary};
            file << rendered;
            // Not a pass: a run that rewrote the goldens has checked nothing,
            // and saying so stops somebody committing an "all green" that only
            // means the files now agree with whatever the code does.
            GTEST_SKIP() << "rewrote " << path.string();
        }

        std::ifstream file{path, std::ios::binary};
        if (!file) {
            ADD_FAILURE() << "no golden at " << path.string()
                          << "\nrun with TYPEIT_UPDATE_GOLDENS=1 to create it, then read it before committing\n"
                          << rendered;
            return;
        }

        std::ostringstream expected;
        expected << file.rdbuf();
        // Printed whole rather than diffed line by line: a terminal screen is
        // read by looking at it, and twenty-four lines fit on one.
        EXPECT_EQ(rendered, expected.str()) << "expected:\n" << expected.str() << "\ngot:\n" << rendered;
    }

}  // namespace typeit::testing
