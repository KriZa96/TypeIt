#include "screens/TextLibraryScreen.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>
#include <string>
#include <utility>
#include <vector>

#include "Bars.h"
#include "ColorQuantizer.h"
#include "Glyphs.h"
#include "Keymap.h"
#include "typeit/app/Json.h"
#include "typeit/core/text/Grapheme.h"
#include "typeit/core/text/TextBuffer.h"

namespace typeit::tui {
    namespace {

        /// Everything above the list: title, blank, the buttons, a blank, a
        /// prompt or message, and the hint bar.
        constexpr std::size_t kChromeRows = 8;
        constexpr std::size_t kMinimumRows = 1;
        constexpr std::size_t kTitleColumns = 34;
        constexpr std::size_t kBarCells = 4;

        /// `title`, cut to `columns` *display* cells and marked where it was
        /// cut.
        ///
        /// Counted in cells rather than bytes or code points: a CJK title is
        /// two cells a character, and cutting it by byte would either overflow
        /// the column or slice a character in half. `TextBuffer` already knows
        /// how to answer this, so it answers it.
        [[nodiscard]] std::string fit(std::string_view title, std::size_t columns) {
            const core::Result<core::TextBuffer> buffer = core::TextBuffer::from_utf8(title);
            if (!buffer || columns == 0) {
                // Not valid UTF-8, which the library should have refused at
                // import. Shown as-is rather than dropped: a row somebody
                // cannot read still tells them which text is broken.
                return std::string{title};
            }

            std::size_t used = 0;
            std::string out;
            for (const core::Grapheme& grapheme: buffer->graphemes()) {
                // The width the segmenter already measured, not one recomputed
                // here — two answers to "how wide is this" is one too many.
                if (used + grapheme.width > columns - 1) {
                    out += "…";
                    return out;
                }
                out += grapheme.view();
                used += grapheme.width;
            }
            // Padded so the columns after it line up whatever the title was.
            out += std::string(columns > used ? columns - used : 0, ' ');
            return out;
        }

        /// A four-cell bar for a fraction, and the number beside it. The bar is
        /// the glance and the number is the answer.
        [[nodiscard]] std::string progress_bar(double fraction, app::GlyphSet glyphs) {
            const auto filled = static_cast<std::size_t>(std::lround(fraction * static_cast<double>(kBarCells)));
            std::string bar;
            for (std::size_t cell = 0; cell < kBarCells; ++cell) {
                bar += glyph(glyphs, cell < filled ? Glyph::ProgressFilled : Glyph::ProgressEmpty);
            }
            return bar;
        }

        [[nodiscard]] std::string percent(double fraction) {
            return fixed_width(std::to_string(static_cast<int>(std::lround(fraction * 100.0))) + "%", 4);
        }

        [[nodiscard]] std::string_view prompt_for(LibraryMode mode) {
            switch (mode) {
                case LibraryMode::Searching:
                    return "search";
                case LibraryMode::Importing:
                    return "import from";
                case LibraryMode::Pasting:
                    return "paste (enter saves)";
                case LibraryMode::Tagging:
                    return "tag";
                case LibraryMode::Confirming:
                    return "delete? type y to confirm";
                case LibraryMode::Browsing:
                    return "";
            }
            return "";
        }

    }  // namespace

    TextLibraryScreen::TextLibraryScreen(const ScreenContext& context) : context_{&context} { reload(); }

    void TextLibraryScreen::reload() {
        rows_.clear();
        const LibrarySource& source = context_->library;
        if (source.records == nullptr) {
            return;
        }

        app::TextFilter filter;
        if (!search_.empty()) {
            filter.search = search_;
        }
        const core::Result<std::vector<app::TextSummary>> texts = source.records->list(filter);
        if (!texts) {
            message_ = core::to_string(texts.error());
            return;
        }

        for (const app::TextSummary& summary: *texts) {
            LibraryRow row{.summary = summary, .progress = {}};
            if (source.service != nullptr) {
                // A text whose progress cannot be read is still worth listing;
                // it simply shows nothing typed.
                if (const core::Result<app::TextProgress> place = source.service->progress(summary.id); place) {
                    row.progress = *place;
                }
            }
            rows_.push_back(std::move(row));
        }
        selected_ = std::min(selected_, rows_.empty() ? 0 : rows_.size() - 1);
        first_row_ = 0;
    }

    void TextLibraryScreen::move_selection(std::int64_t by) {
        if (rows_.empty()) {
            return;
        }
        const auto last = static_cast<std::int64_t>(rows_.size() - 1);
        selected_ =
                static_cast<std::size_t>(std::clamp(static_cast<std::int64_t>(selected_) + by, std::int64_t{0}, last));

        const std::size_t page = context_->size.rows > kChromeRows ? context_->size.rows - kChromeRows : kMinimumRows;
        if (selected_ < first_row_) {
            first_row_ = selected_;
        } else if (selected_ >= first_row_ + page) {
            first_row_ = selected_ - page + 1;
        }
    }

    std::optional<core::TextId> TextLibraryScreen::take_chosen() {
        std::optional<core::TextId> taken = chosen_;
        chosen_.reset();
        return taken;
    }

    void TextLibraryScreen::begin(LibraryMode mode) {
        mode_ = mode;
        message_.clear();
        // The search box keeps what is in it; every other prompt starts empty,
        // because a tag box pre-filled with yesterday's tag is a tag somebody
        // applies by accident.
        typed_ = mode == LibraryMode::Searching ? search_ : std::string{};
    }

    void TextLibraryScreen::do_import() {
        app::TextLibraryService* const service = context_->library.service;
        if (service == nullptr) {
            message_ = "importing is not available in this build";
            return;
        }
        if (const core::Result<app::ImportOutcome> imported = service->import_file(typed_); imported) {
            message_ =
                    imported->already_present ? "that text was already in the library" : "imported \"" + typed_ + "\"";
        } else {
            message_ = core::to_string(imported.error());
        }
    }

    void TextLibraryScreen::do_paste() {
        app::TextLibraryService* const service = context_->library.service;
        if (service == nullptr) {
            message_ = "importing is not available in this build";
            return;
        }
        // No size check here: `kMaxImportBytes` already names the limit and
        // reports it, and a screen with its own opinion would be a second
        // answer that could disagree with the first.
        const core::Result<app::ImportOutcome> imported = service->import_text(typed_, app::TextSource::Paste);
        message_ = imported ? "pasted text saved" : core::to_string(imported.error());
    }

    void TextLibraryScreen::do_tag() {
        app::ITextLibraryRepository* const records = context_->library.records;
        if (records == nullptr || rows_.empty()) {
            return;
        }
        const core::Status tagged = records->tag(rows_.at(selected_).summary.id, typed_);
        message_ = tagged ? "tagged \"" + typed_ + "\"" : core::to_string(tagged.error());
    }

    void TextLibraryScreen::do_delete() {
        app::ITextLibraryRepository* const records = context_->library.records;
        if (records == nullptr || rows_.empty()) {
            return;
        }
        // Only an explicit `y`. Enter on a confirmation nobody read is how
        // somebody deletes a book they spent a month typing.
        if (typed_ != "y") {
            message_ = "not deleted";
            return;
        }
        const core::Status removed = records->remove(rows_.at(selected_).summary.id);
        message_ =
                removed ? "deleted, with its tags and bookmark; past runs are kept" : core::to_string(removed.error());
    }

    void TextLibraryScreen::commit() {
        switch (mode_) {
            case LibraryMode::Importing:
                do_import();
                break;
            case LibraryMode::Pasting:
                do_paste();
                break;
            case LibraryMode::Tagging:
                do_tag();
                break;
            case LibraryMode::Confirming:
                do_delete();
                break;
            case LibraryMode::Searching:
            case LibraryMode::Browsing:
                // Nothing to do either way. Searching has already applied
                // itself on every keystroke, so enter just closes the box; and
                // browsing has no prompt open to commit.
                break;
        }

        mode_ = LibraryMode::Browsing;
        typed_.clear();
        reload();
    }

    bool TextLibraryScreen::handle_prompt(const ftxui::Event& event) {
        if (event == ftxui::Event::Return) {
            commit();
            return true;
        }
        if (event == ftxui::Event::Escape) {
            // Cancelling discards. A paste half typed and then abandoned is not
            // a text somebody wanted.
            mode_ = LibraryMode::Browsing;
            typed_.clear();
            return true;
        }
        if (event == ftxui::Event::Backspace) {
            if (!typed_.empty()) {
                typed_.pop_back();
            }
        } else if (event.is_character()) {
            // The whole character, not its first byte: a paste arrives as one
            // event carrying several, and `é` is two of them.
            typed_ += event.character();
        } else {
            // Anything else is swallowed rather than acted on: a prompt that
            // let arrow keys move the list behind it would apply a tag to a
            // text the user is no longer looking at.
            return true;
        }

        if (mode_ == LibraryMode::Searching) {
            // Live, as the issue asks. The query is over a local library and
            // the alternative — searching only on enter — is a filter somebody
            // has to remember to submit.
            search_ = typed_;
            reload();
        }
        return true;
    }

    bool TextLibraryScreen::on_event(ftxui::Event event) {
        if (mode_ != LibraryMode::Browsing) {
            return handle_prompt(event);
        }

        if (event == ftxui::Event::ArrowDown) {
            move_selection(1);
            return true;
        }
        if (event == ftxui::Event::ArrowUp) {
            move_selection(-1);
            return true;
        }
        if (event == ftxui::Event::Return && !rows_.empty()) {
            chosen_ = rows_.at(selected_).summary.id;
            return true;
        }

        if (event.is_character() && event.character().size() == 1) {
            switch (event.character().front()) {
                case '/':
                    begin(LibraryMode::Searching);
                    return true;
                case 'i':
                    begin(LibraryMode::Importing);
                    return true;
                case 'p':
                    begin(LibraryMode::Pasting);
                    return true;
                case 't':
                    if (!rows_.empty()) {
                        begin(LibraryMode::Tagging);
                    }
                    return true;
                case 'd':
                    if (!rows_.empty()) {
                        begin(LibraryMode::Confirming);
                    }
                    return true;
                default:
                    return false;
            }
        }
        return false;
    }

    ftxui::Element TextLibraryScreen::list_rows(const Styling& accent, const Styling& muted) const {
        if (rows_.empty()) {
            // The first-run state, and the state a search with no matches
            // leaves. Two sentences rather than one, because they are two
            // different situations and the way out of each is different.
            const std::string_view said = search_.empty()
                                                  ? "  Nothing here yet. Press i to import a file, or p to paste."
                                                  : "  Nothing matches that search.";
            return ftxui::text(std::string{said}) | ftxui::color(muted.color);
        }

        const std::size_t page = context_->size.rows > kChromeRows ? context_->size.rows - kChromeRows : kMinimumRows;
        const std::size_t last = std::min(rows_.size(), first_row_ + page);

        std::vector<ftxui::Element> lines;
        for (std::size_t at = first_row_; at < last; ++at) {
            const LibraryRow& row = rows_.at(at);
            const bool here = at == selected_;
            std::string line = here ? "  > " : "    ";
            line += fit(row.summary.title, kTitleColumns);
            line += fixed_width(std::to_string(row.summary.word_count), 7) + " words  ";
            line += "diff " + fixed_width(row.summary.difficulty.has_value()
                                                  ? app::json::number(std::round(*row.summary.difficulty * 10.0) / 10.0)
                                                  : "-",
                                          4);
            line += "  " + progress_bar(row.progress.fraction, context_->capabilities.glyphs);
            line += " " + percent(row.progress.fraction);
            lines.push_back(ftxui::text(line) | ftxui::color(here ? accent.color : muted.color));
        }
        return ftxui::vbox(std::move(lines));
    }

    ftxui::Element TextLibraryScreen::prompt_row(const Styling& accent, const Styling& muted) const {
        if (mode_ == LibraryMode::Browsing) {
            return message_.empty() ? ftxui::text("") : ftxui::text("  " + message_) | ftxui::color(muted.color);
        }
        return ftxui::hbox({
                ftxui::text("  " + std::string{prompt_for(mode_)} + "  ") | ftxui::color(muted.color),
                ftxui::text(typed_ + "_") | ftxui::color(accent.color),
        });
    }

    ftxui::Element TextLibraryScreen::render() {
        const app::ColorDepth depth = context_->capabilities.color;
        const Styling accent = style_for(*context_->theme, app::ThemeColor::Accent, depth);
        const Styling muted = style_for(*context_->theme, app::ThemeColor::Muted, depth);

        std::vector<ftxui::Element> rows;
        rows.push_back(ftxui::hbox({
                ftxui::text("Texts") | ftxui::color(accent.color),
                ftxui::text(search_.empty() ? "" : "   search: " + search_) | ftxui::color(muted.color),
        }));
        rows.push_back(ftxui::text(""));
        rows.push_back(list_rows(accent, muted));
        rows.push_back(ftxui::text(""));
        rows.push_back(ftxui::text("  i import   p paste   t tag   d delete   / search") | ftxui::color(muted.color));
        rows.push_back(prompt_row(accent, muted));
        rows.push_back(ftxui::text(""));
        rows.push_back(key_hint_bar({{.action = Action::QuitOrBack, .label = "back"}}, *context_->keymap,
                                    *context_->theme, depth));
        return ftxui::vbox(std::move(rows));
    }

}  // namespace typeit::tui
