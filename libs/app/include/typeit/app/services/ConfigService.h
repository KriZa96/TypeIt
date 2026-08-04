// The configuration, as the rest of the application sees it.
//
// One place holds the current settings, one place validates them before they
// are written, and anything that cares about a change is told. The store below
// knows about files; this knows about rules.
#ifndef TYPEIT_APP_SERVICES_CONFIGSERVICE_H
#define TYPEIT_APP_SERVICES_CONFIGSERVICE_H

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

#include "typeit/app/ports/IConfigStore.h"
#include "typeit/core/config/Config.h"
#include "typeit/core/util/Result.h"

namespace typeit::app {

    class ConfigService {
    public:
        /// The store must outlive the service. The composition root owns both.
        explicit ConfigService(IConfigStore& store) : store_{&store} {}

        /// Reads the file and keeps what it got.
        ///
        /// A failure leaves the defaults in place **and is returned**. Both
        /// halves matter: the application must still start — a broken config is
        /// not a reason to be unusable — and the person whose file could not be
        /// read must be told, or they will spend an evening wondering why their
        /// settings do nothing.
        [[nodiscard]] core::Status load();

        [[nodiscard]] const core::Config& config() const noexcept { return config_; }

        /// What the last load complained about: unknown keys, values out of
        /// range. Empty after a clean load.
        [[nodiscard]] const std::vector<std::string>& warnings() const noexcept { return warnings_; }

        /// Validates, writes, and only then adopts and announces.
        ///
        /// An invalid config is refused rather than written, so the file on
        /// disk is never something this program would itself reject. A failed
        /// write leaves the in-memory settings exactly as they were: a settings
        /// screen that says "saved" over a full disk has lied.
        [[nodiscard]] core::Status save(const core::Config& config);

        using Observer = std::function<void(const core::Config&)>;

        /// Called on every successful save, with the new configuration. Not on
        /// load: at load nothing has changed yet, and an observer that fires
        /// during construction is an observer nobody has registered.
        void on_change(Observer observer) { observers_.push_back(std::move(observer)); }

        [[nodiscard]] std::size_t observer_count() const noexcept { return observers_.size(); }

    private:
        IConfigStore* store_;
        core::Config config_;
        std::vector<std::string> warnings_;
        std::vector<Observer> observers_;
    };

}  // namespace typeit::app

#endif  // TYPEIT_APP_SERVICES_CONFIGSERVICE_H
