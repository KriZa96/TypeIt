#include "typeit/infra/term/StandardInput.h"

#include <cstdio>
#include <filesystem>
#include <istream>
#include <sstream>
#include <string>

#include "typeit/core/util/Result.h"

namespace typeit::infra {

    core::Result<std::string> read_stream(std::istream& input) {
        std::ostringstream contents;
        contents << input.rdbuf();

        // `failbit` on an empty stream is not a failure: `rdbuf()` sets it when
        // there was nothing to insert, and "the pipe was empty" is a case the
        // importer reports far better than this can. `badbit` is a real one —
        // the read broke part way, and half an article imported as a whole one
        // is the failure nobody notices.
        if (input.bad()) {
            return core::fail(core::ErrorCode::FileUnreadable, "standard input: the read failed part way through");
        }
        return contents.str();
    }

    std::filesystem::path controlling_terminal() {
#ifdef _WIN32
        // The console's input device by name. `CON` would be the console
        // generally; `CONIN$` is specifically its input, which is what stdin
        // has to become.
        return "CONIN$";
#else
        return "/dev/tty";
#endif
    }

    core::Status reattach_input(const std::filesystem::path& device) {
        // `freopen` rather than opening a new stream: FTXUI reads the process's
        // `stdin`, so it is that handle which has to change. Anything that
        // opened a second stream would leave the library still looking at the
        // drained pipe.
        //
        // `freopen` on `stdin` returns `stdin` itself, which this does not own
        // and must not close — changing the stream the C runtime and FTXUI
        // already hold, in place, is the entire point.
        //
        // `freopen_s` on Windows, where the plain one is a deprecation error
        // under `/W4 -Werror` on both MSVC and clang-cl. Same call, same
        // in-place reopen; the difference is which of them the C runtime
        // considers safe, and there is no portable spelling of "I meant it".
#ifdef _WIN32
        FILE* reopened = nullptr;
        // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
        if (freopen_s(&reopened, device.string().c_str(), "r", stdin) != 0) {
#else
        // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
        if (std::freopen(device.string().c_str(), "r", stdin) == nullptr) {
#endif
            return core::fail(core::ErrorCode::UnsupportedTerminal,
                              device.string() + ": there is no terminal to read the keyboard from");
        }
        return {};
    }

}  // namespace typeit::infra
