// Writing JSON by hand, in the two places that have to (TI-069, TI-075).
//
// No dependency, because the shape written here is fixed and known: a flat
// object of numbers, strings and one array. A JSON *reader* would be a
// different argument — parsing arbitrary input is where a library earns its
// keep — but nothing in this program reads JSON.
//
// It lives in a header rather than in each writer's translation unit because
// both exports promise the same thing: **byte-identical output for the same
// data**. Two spellings of "how a double is written" would drift the day one
// of them was tuned, and the promise would break somewhere nobody was looking.
#ifndef TYPEIT_APP_JSON_H
#define TYPEIT_APP_JSON_H

#include <string>
#include <string_view>

namespace typeit::app::json {

    /// Appends `value` as a quoted JSON string.
    ///
    /// Non-ASCII passes through as the UTF-8 it already is rather than as
    /// `\u` escapes: the file is UTF-8 and every parser reads it.
    void append_string(std::string& out, std::string_view value);

    /// The shortest round-trippable spelling of `value`, and never `nan` or
    /// `inf` — which are not JSON, and which no spreadsheet reads either.
    ///
    /// Also what the CSV export writes, deliberately: the two exports are the
    /// same numbers in different punctuation, and a reader comparing them
    /// should not find one saying 99.5 and the other 99.500000.
    [[nodiscard]] std::string number(double value);

}  // namespace typeit::app::json

#endif  // TYPEIT_APP_JSON_H
