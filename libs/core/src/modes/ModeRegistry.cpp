#include "typeit/core/modes/ModeRegistry.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "typeit/core/modes/IMode.h"
#include "typeit/core/util/Result.h"

namespace typeit::core {

    void ModeRegistry::register_mode(std::string_view id, Factory factory) {
        factories_.insert_or_assign(std::string{id}, std::move(factory));
    }

    Result<std::unique_ptr<IMode>> ModeRegistry::create(std::string_view id) const {
        const auto found = factories_.find(id);
        if (found == factories_.end()) {
            return fail(ErrorCode::UnknownMode, std::string{id});
        }
        return found->second();
    }

    bool ModeRegistry::contains(std::string_view id) const { return factories_.contains(id); }

    std::vector<std::string> ModeRegistry::ids() const {
        std::vector<std::string> ids;
        ids.reserve(factories_.size());
        for (const auto& [id, factory]: factories_) {
            ids.push_back(id);
        }
        return ids;
    }

}  // namespace typeit::core
