#include "typeit/app/services/ConfigService.h"

#include <utility>

#include "typeit/app/ports/IConfigStore.h"
#include "typeit/core/config/Config.h"
#include "typeit/core/config/Validation.h"
#include "typeit/core/util/Result.h"

namespace typeit::app {

    core::Status ConfigService::load() {
        warnings_.clear();

        core::Result<LoadedConfig> loaded = store_->load();
        if (!loaded) {
            // The defaults stay, and the caller is told why. Starting with
            // default settings beats not starting; silence beats neither.
            config_ = core::Config{};
            return std::unexpected{loaded.error()};
        }

        config_ = std::move(loaded->config);
        warnings_ = std::move(loaded->warnings);

        // A file can be syntactically fine and still say something impossible —
        // by hand, or by a downgrade after a newer version wrote it. The store
        // reverts individual fields it can attribute; anything left is caught
        // here, and the defaults are the answer.
        if (const core::Status valid = core::validate(config_); !valid) {
            config_ = core::Config{};
            warnings_.push_back(valid.error().context + "; every setting was reset to its default");
        }
        return {};
    }

    core::Status ConfigService::save(const core::Config& config) {
        // Validate first: the file on disk should never be something this
        // program would itself refuse to load.
        if (const core::Status valid = core::validate(config); !valid) {
            return valid;
        }
        if (const core::Status written = store_->save(config); !written) {
            // Nothing adopted, nothing announced. A settings screen that says
            // "saved" over a full disk has lied to somebody.
            return written;
        }

        config_ = config;
        for (const Observer& observer: observers_) {
            observer(config_);
        }
        return {};
    }

}  // namespace typeit::app
