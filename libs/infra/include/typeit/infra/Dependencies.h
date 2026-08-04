// What this build was actually linked against.
//
// Every dependency can arrive three ways — a distro package, vcpkg, or a
// pinned download (BUILD section 4) — so "which SQLite is this?" is a question
// with a real answer only at runtime. `--doctor` reports these, and a bug
// report that includes them is one nobody has to guess at.
//
// Declared here rather than by exposing the libraries themselves: the version
// of SQLite is a string, and a string does not drag sqlite3.h into `app`.
#ifndef TYPEIT_INFRA_DEPENDENCIES_H
#define TYPEIT_INFRA_DEPENDENCIES_H

#include <string_view>

namespace typeit::infra {

    /// The SQLite library this binary will call, as reported by SQLite itself
    /// at runtime — not the version the build system asked for, which can
    /// differ when the loader finds another one.
    [[nodiscard]] std::string_view sqlite_version();

    /// The toml++ this binary was compiled against. Header-only, so this is a
    /// compile-time constant and cannot disagree with reality.
    [[nodiscard]] std::string_view toml_version();

}  // namespace typeit::infra

#endif  // TYPEIT_INFRA_DEPENDENCIES_H
