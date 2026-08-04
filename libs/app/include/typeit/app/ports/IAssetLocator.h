// Where the bundled corpora and themes are (defect C2, TECHNICAL section 4.1).
#ifndef TYPEIT_APP_PORTS_IASSETLOCATOR_H
#define TYPEIT_APP_PORTS_IASSETLOCATOR_H

#include <filesystem>
#include <string_view>

#include "typeit/core/util/Result.h"

namespace typeit::app {

    class IAssetLocator {
    public:
        IAssetLocator() = default;
        virtual ~IAssetLocator();
        IAssetLocator(const IAssetLocator&) = delete;
        IAssetLocator& operator=(const IAssetLocator&) = delete;
        IAssetLocator(IAssetLocator&&) = delete;
        IAssetLocator& operator=(IAssetLocator&&) = delete;

        /// `locate("texts")`, `locate("themes")`. Failing is not fatal: the
        /// bundled assets are a convenience, and a user with imported texts of
        /// their own loses nothing by them being absent.
        [[nodiscard]] virtual core::Result<std::filesystem::path> locate(std::string_view kind) const = 0;
    };

}  // namespace typeit::app

#endif  // TYPEIT_APP_PORTS_IASSETLOCATOR_H
