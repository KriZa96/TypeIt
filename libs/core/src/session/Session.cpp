#include "typeit/core/session/Session.h"

#include <cassert>
#include <cstddef>
#include <memory>
#include <string>
#include <utility>

#include "typeit/core/text/Grapheme.h"
#include "typeit/core/text/TextBuffer.h"
#include "typeit/core/util/Result.h"
#include "typeit/core/util/Units.h"

namespace typeit::core {

    Result<std::unique_ptr<Session>> Session::create(Parts parts, Millis started_at) {
        assert(parts.mode != nullptr && "a session needs a mode to know when it is over");
        assert(parts.provider != nullptr && "a session needs a provider to have anything to type");

        // One chunk, once. A provider with more to give is a text being typed
        // over several sittings (`ChunkedProvider`), not a run that grows while
        // it is being played: the model holds a span into the buffer, and
        // appending to it mid-run would move the text out from under the
        // cursor. Endless mode needs a refill and is Phase 7's problem, where
        // rebasing the model is the design rather than an afterthought.
        const std::string chunk = parts.provider->next_chunk();

        Result<TextBuffer> text = TextBuffer::from_utf8(chunk);
        if (!text) {
            return std::unexpected{text.error()};
        }

        // Not make_unique: the constructor is private, which is the point.
        std::unique_ptr<Session> session{new Session{std::move(*text), std::move(parts), started_at}};
        session->mode_->on_start(started_at, session->model_);
        return session;
    }

    Session::Session(TextBuffer text, Parts parts, Millis started_at) :
        text_{std::move(text)}, mode_{std::move(parts.mode)}, provider_{std::move(parts.provider)},
        model_{text_, parts.rules}, started_at_{started_at} {}

    void Session::on_key(const Grapheme& grapheme, Millis at) {
        const std::size_t before = model_.log().size();
        model_.type(grapheme, at);
        notify_if_logged(before);
    }

    void Session::on_backspace(Millis at) {
        const std::size_t before = model_.log().size();
        model_.backspace(at);
        notify_if_logged(before);
    }

    void Session::on_tick(Millis now) { mode_->on_tick(now, model_); }

    void Session::notify_if_logged(std::size_t before) {
        // A keystroke the rules refused, or one aimed past the end of the text,
        // changes nothing and logs nothing. Telling the mode about it would
        // have a word-count run credit a word the typist never typed, and a
        // race react to a key the game ignored.
        if (model_.log().size() == before) {
            return;
        }
        mode_->on_keystroke(model_.log().events().back(), model_);
    }

}  // namespace typeit::core
