// The ports' destructors, defined once.
//
// Out of line on purpose: a class with no key function has its vtable and its
// type information emitted in every translation unit that uses it. One
// definition here means one vtable per port, which is the entire reason this
// library has a source file at all.

#include "typeit/app/ports/Ports.h"

#include <optional>
#include <string_view>

#include "typeit/app/records/TextLibrary.h"

namespace typeit::app {

    IHistoryRepository::~IHistoryRepository() = default;
    ITextLibraryRepository::~ITextLibraryRepository() = default;
    IConfigStore::~IConfigStore() = default;
    IAssetLocator::~IAssetLocator() = default;
    IFileSystem::~IFileSystem() = default;

    std::string_view to_string(TextSource source) {
        switch (source) {
            case TextSource::Builtin:
                return "builtin";
            case TextSource::File:
                return "file";
            case TextSource::Paste:
                return "paste";
            case TextSource::Stdin:
                return "stdin";
        }
        // Unreachable: every enumerator returns above. Kept because a switch
        // that falls off the end is undefined behaviour if the enum ever holds
        // a value cast in from outside.
        return "paste";
    }

    std::optional<TextSource> text_source_from(std::string_view name) {
        // The database has a CHECK constraint naming exactly these, so a
        // spelling that is not here would be rejected on the way in anyway —
        // better to say so before the write than after it.
        if (name == "builtin") {
            return TextSource::Builtin;
        }
        if (name == "file") {
            return TextSource::File;
        }
        if (name == "paste") {
            return TextSource::Paste;
        }
        if (name == "stdin") {
            return TextSource::Stdin;
        }
        return std::nullopt;
    }

}  // namespace typeit::app
