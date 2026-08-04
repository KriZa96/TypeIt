// A private config and data directory for one test, and the environment
// variables that point the application at them (TI-065).
//
// Every test that touches persistence takes one of these. The rule it enforces
// is absolute: **no test ever reads or writes a developer's real history**. A
// suite that scribbles in `~/.local/share/typeit` is a suite nobody can run
// twice, and one that deletes it is worse.
//
// It sets the real process environment, because that is what the code under
// test reads through `system_environment()`. That makes it single-threaded by
// contract: ctest parallelises across processes, which is safe, and two of
// these in one process nest rather than collide.
#ifndef TYPEIT_TESTING_TEMPENV_H
#define TYPEIT_TESTING_TEMPENV_H

#include <filesystem>
#include <optional>
#include <string>

namespace typeit::testing {

    class TempEnv {
    public:
        /// Creates a unique directory and points `TYPEIT_CONFIG_DIR` and
        /// `TYPEIT_DATA_DIR` at subdirectories of it.
        TempEnv();

        /// Restores whatever the variables held before — including "unset",
        /// which is not the same as empty — and removes the directory. Runs on
        /// a failing test too, because destructors do not care about
        /// assertions.
        ~TempEnv();

        TempEnv(const TempEnv&) = delete;
        TempEnv& operator=(const TempEnv&) = delete;
        TempEnv(TempEnv&&) = delete;
        TempEnv& operator=(TempEnv&&) = delete;

        [[nodiscard]] const std::filesystem::path& root() const noexcept { return root_; }
        [[nodiscard]] std::filesystem::path config() const { return root_ / "config"; }
        [[nodiscard]] std::filesystem::path data() const { return root_ / "data"; }

    private:
        std::filesystem::path root_;
        std::optional<std::string> previous_config_;
        std::optional<std::string> previous_data_;
    };

    /// Reads a variable from the real process environment. Exposed so a test
    /// can assert what a `TempEnv` did, and what it undid.
    [[nodiscard]] std::optional<std::string> read_environment(const char* name);

}  // namespace typeit::testing

#endif  // TYPEIT_TESTING_TEMPENV_H
