#include "typeit/core/metrics/ErrorMap.h"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "typeit/core/session/Keystroke.h"
#include "typeit/core/session/KeystrokeLog.h"
#include "typeit/core/text/Grapheme.h"
#include "typeit/core/text/TextBuffer.h"
#include "typeit/core/util/Units.h"

namespace typeit::core {

    ErrorMap error_map(const KeystrokeLog& log, const TextBuffer& target) {
        ErrorMap errors;

        std::vector<std::optional<Grapheme>> first_attempt(target.size());
        std::optional<std::size_t> furthest;

        for (const Keystroke& event: log.events()) {
            if (event.kind != KeystrokeKind::Character) {
                continue;
            }
            if (event.target >= target.size()) {
                ++errors.insertions[std::string{event.typed.view()}];
                continue;
            }
            furthest = furthest.has_value() ? std::max(*furthest, std::size_t{event.target}) : event.target;
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access) -- bounds checked
            if (!first_attempt[event.target].has_value()) {
                // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access) -- bounds checked
                first_attempt[event.target] = event.typed;
            }
        }

        if (!furthest.has_value()) {
            return errors;
        }

        for (std::size_t position = 0; position <= *furthest; ++position) {
            const Grapheme& expected = target.at(GraphemeIndex{position});
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access) -- position <= furthest
            const std::optional<Grapheme>& typed = first_attempt[position];
            if (!typed.has_value()) {
                ++errors.omissions[std::string{expected.view()}];
                continue;
            }
            if (!(*typed == expected)) {
                ++errors.substitutions[{std::string{expected.view()}, std::string{typed->view()}}];
            }
        }

        return errors;
    }

}  // namespace typeit::core
