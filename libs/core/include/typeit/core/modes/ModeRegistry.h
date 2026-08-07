// Id → mode, so that adding a mode is a registration rather than a new case in
// somebody else's switch (ADR-008).
#ifndef TYPEIT_CORE_MODES_MODEREGISTRY_H
#define TYPEIT_CORE_MODES_MODEREGISTRY_H

#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "typeit/core/modes/IMode.h"
#include "typeit/core/util/Result.h"
#include "typeit/core/util/Units.h"

namespace typeit::core {

    /// The one number a mode is chosen *with*: thirty seconds, fifty words.
    ///
    /// It travels beside the id rather than inside it because the menu picks it
    /// at the last moment. Registering "timed" with a duration baked in — which
    /// is what this was — meant the seconds field on the menu changed the
    /// number recorded in the history and nothing else.
    ///
    /// Zero means "whatever the factory was registered with", so a mode that
    /// takes no parameter, and a caller that has none to give, both work
    /// without a special case.
    struct ModeParams {
        Millis duration{0};
        std::size_t words = 0;
    };

    class ModeRegistry {
    public:
        using Factory = std::function<std::unique_ptr<IMode>(const ModeParams&)>;

        /// Registering the same id twice replaces the first: a caller that
        /// wants to override a built-in mode should not have to remove it
        /// first, and two modes claiming one id is a bug either way.
        void register_mode(std::string_view id, Factory factory);

        /// For a mode with nothing to parameterise — quote and zen, and most
        /// tests. An overload rather than a defaulted argument, so the caller
        /// writes the lambda it means and no `(void)params` appears anywhere.
        void register_mode(std::string_view id, std::function<std::unique_ptr<IMode>()> factory);

        /// `ErrorCode::UnknownMode`, naming the id, rather than a null pointer
        /// the caller might forget to check — a configuration file with a typo
        /// in it is a user error, and users get messages.
        [[nodiscard]] Result<std::unique_ptr<IMode>> create(std::string_view id, const ModeParams& params = {}) const;

        [[nodiscard]] bool contains(std::string_view id) const;

        /// Sorted, so a menu built from this does not reorder itself between
        /// runs.
        [[nodiscard]] std::vector<std::string> ids() const;

        [[nodiscard]] std::size_t size() const noexcept { return factories_.size(); }

    private:
        std::map<std::string, Factory, std::less<>> factories_;
    };

}  // namespace typeit::core

#endif  // TYPEIT_CORE_MODES_MODEREGISTRY_H
