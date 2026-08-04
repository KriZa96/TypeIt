#include <gtest/gtest.h>
#include <string_view>
#include <type_traits>

#include "typeit/app/ports/Ports.h"
#include "typeit/app/records/History.h"
#include "typeit/app/records/TextLibrary.h"
#include "typeit/core/util/Units.h"

namespace typeit::app {
    namespace {

        // A port is an interface and nothing else. These are the two ways that
        // stops being true — somebody adds a member to "just cache this", or
        // defines a destructor that is not virtual — and both are compile-time
        // facts rather than review comments.
        template<typename Port>
        constexpr bool is_a_port() {
            static_assert(std::is_abstract_v<Port>, "a port must have no implementation to inherit");
            static_assert(std::has_virtual_destructor_v<Port>,
                          "deleting through a base without one leaks the derived part");
            // An interface with no data has nothing to copy, so an empty base
            // is exactly one byte of nothing.
            static_assert(std::is_empty_v<Port> || sizeof(Port) <= sizeof(void*),
                          "a port must carry no data members; state belongs in the implementation");
            return true;
        }

        static_assert(is_a_port<IHistoryRepository>());
        static_assert(is_a_port<ITextLibraryRepository>());
        static_assert(is_a_port<IConfigStore>());
        static_assert(is_a_port<IAssetLocator>());
        static_assert(is_a_port<IFileSystem>());
        static_assert(is_a_port<IClock>());

        // Copying a port would copy an adapter's identity — a database
        // connection, a file handle — into something that owns neither.
        static_assert(!std::is_copy_constructible_v<IHistoryRepository>);
        static_assert(!std::is_move_constructible_v<IHistoryRepository>);

        TEST(PortsTest, TheClockIsReExportedUnderTheAppNamespace) {
            // It is a port too — the one the domain declares for itself, because
            // time is an input to the model rather than a service around it.
            static_assert(std::is_same_v<IClock, core::IClock>);
            SUCCEED();
        }

        TEST(TextSourceTest, EverySourceRoundTripsThroughItsName) {
            for (const TextSource source:
                 {TextSource::Builtin, TextSource::File, TextSource::Paste, TextSource::Stdin}) {
                EXPECT_EQ(text_source_from(to_string(source)), source) << to_string(source);
            }
        }

        TEST(TextSourceTest, TheNamesAreTheOnesTheDatabaseChecksFor) {
            // The `source` column has a CHECK constraint naming exactly these,
            // so a spelling that disagreed here would be rejected on write.
            EXPECT_EQ(to_string(TextSource::Builtin), "builtin");
            EXPECT_EQ(to_string(TextSource::File), "file");
            EXPECT_EQ(to_string(TextSource::Paste), "paste");
            EXPECT_EQ(to_string(TextSource::Stdin), "stdin");
        }

        TEST(TextSourceTest, AnUnknownNameIsRefusedBeforeTheWriteRatherThanAfter) {
            EXPECT_FALSE(text_source_from("telepathy").has_value());
            EXPECT_FALSE(text_source_from("").has_value());
            EXPECT_FALSE(text_source_from("File").has_value()) << "the constraint is case-sensitive";
        }

        TEST(RecordsTest, TheDefaultsAreTheEmptyRunAndTheEmptyLibrary) {
            // Every record is default-constructible into something harmless, so
            // a partially filled one is a compile error away from being caught
            // rather than a garbage value away from being written.
            const SessionRecord session;
            EXPECT_FALSE(session.completed) << "an unfinished record is not a completed run";
            EXPECT_FALSE(session.text_id.has_value());
            EXPECT_TRUE(session.timeline.empty());

            const Aggregates empty;
            EXPECT_EQ(empty.sessions, 0U);
            EXPECT_EQ(empty.mean_net_wpm.value, 0.0) << "zeros over an empty range, never NaN";

            const HistoryFilter filter;
            EXPECT_FALSE(filter.mode.has_value()) << "no filter means every run";
            EXPECT_TRUE(filter.completed_only) << "abandoned runs stay out of trends unless asked for";
        }

        TEST(RecordsTest, DaysAndMillisCannotBeConfused) {
            // The point of the unit: thirty days and thirty milliseconds must
            // not be interchangeable at a call site that takes a window.
            static_assert(!std::is_convertible_v<core::Days, core::Millis>);
            static_assert(!std::is_convertible_v<core::Millis, core::Days>);
            EXPECT_EQ(core::Days{30}, core::Days{30});
        }

    }  // namespace
}  // namespace typeit::app
