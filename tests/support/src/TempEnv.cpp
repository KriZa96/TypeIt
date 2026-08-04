#include "typeit/testing/TempEnv.h"

#include <atomic>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <optional>
#include <string>
#include <system_error>

#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

namespace typeit::testing {
    namespace {

        void set_environment(const char* name, const std::optional<std::string>& value) {
#ifdef _WIN32
            // _putenv_s with an empty value removes the variable on Windows,
            // which is exactly the "restore to unset" case.
            _putenv_s(name, value.has_value() ? value->c_str() : "");
#else
            if (value.has_value()) {
                // NOLINTNEXTLINE(concurrency-mt-unsafe) -- single-threaded by contract; see the header
                setenv(name, value->c_str(), 1);
            } else {
                // NOLINTNEXTLINE(concurrency-mt-unsafe) -- single-threaded by contract; see the header
                unsetenv(name);
            }
#endif
        }

        /// Unique per process and per fixture. The counter is what stops two
        /// fixtures alive at once from sharing a directory; the process id is
        /// what stops two ctest workers from doing the same.
        std::filesystem::path unique_root() {
            static std::atomic<unsigned> counter{0};
#ifdef _WIN32
            const auto process = static_cast<unsigned long long>(_getpid());
#else
            const auto process = static_cast<unsigned long long>(getpid());
#endif
            return std::filesystem::temp_directory_path() /
                   ("typeit-test-" + std::to_string(process) + "-" + std::to_string(counter.fetch_add(1)));
        }

    }  // namespace

    std::optional<std::string> read_environment(const char* name) {
#ifdef _WIN32
        // getenv is deprecated by the Windows CRT. _dupenv_s allocates, so the
        // buffer is owned and freed here.
        char* value = nullptr;
        std::size_t size = 0;
        if (_dupenv_s(&value, &size, name) != 0 || value == nullptr) {
            std::free(value);
            return std::nullopt;
        }
        std::string copied{value};
        std::free(value);
        return copied;
#else
        // NOLINTNEXTLINE(concurrency-mt-unsafe) -- single-threaded by contract; see the header
        const char* value = std::getenv(name);
        if (value == nullptr) {
            return std::nullopt;
        }
        return std::string{value};
#endif
    }

    TempEnv::TempEnv() :
        root_{unique_root()}, previous_config_{read_environment("TYPEIT_CONFIG_DIR")},
        previous_data_{read_environment("TYPEIT_DATA_DIR")} {
        std::filesystem::remove_all(root_);
        std::filesystem::create_directories(config());
        std::filesystem::create_directories(data());

        set_environment("TYPEIT_CONFIG_DIR", config().string());
        set_environment("TYPEIT_DATA_DIR", data().string());
    }

    TempEnv::~TempEnv() {
        set_environment("TYPEIT_CONFIG_DIR", previous_config_);
        set_environment("TYPEIT_DATA_DIR", previous_data_);

        std::error_code ignored;
        // A directory that cannot be removed is the operating system's
        // problem, not a reason to throw out of a destructor and take an
        // otherwise passing test down with it.
        std::filesystem::remove_all(root_, ignored);
    }

}  // namespace typeit::testing
