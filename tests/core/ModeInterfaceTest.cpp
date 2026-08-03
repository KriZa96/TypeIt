#include <cstddef>
#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "typeit/core/modes/IMode.h"
#include "typeit/core/modes/ModeRegistry.h"
#include "typeit/core/session/Keystroke.h"
#include "typeit/core/session/TypingModel.h"
#include "typeit/core/text/TextBuffer.h"
#include "typeit/core/util/Result.h"
#include "typeit/core/util/Units.h"
#include "typeit/testing/ModeDriver.h"
#include "typeit/testing/SpyMode.h"
#include "typeit/testing/TypingRun.h"

namespace typeit::core {
    namespace {

        TEST(ModeInterfaceTest, TheHooksArriveInTheOrderTheyHappened) {
            testing::SpyMode mode;
            testing::ModeDriver driver{"ab", mode};

            driver.type("a");
            driver.tick();
            driver.type("b");

            EXPECT_EQ(mode.calls, (std::vector<std::string>{"on_start", "on_keystroke", "on_tick", "on_keystroke"}));
            EXPECT_EQ(mode.keystrokes, 2U);
            EXPECT_EQ(mode.ticks, 1U);
        }

        TEST(ModeInterfaceTest, OnStartComesBeforeAnyKeystroke) {
            testing::SpyMode mode;
            const testing::ModeDriver driver{"ab", mode};

            ASSERT_FALSE(mode.calls.empty());
            EXPECT_EQ(mode.calls.front(), "on_start");
            EXPECT_EQ(mode.keystrokes, 0U);
        }

        TEST(ModeInterfaceTest, TicksArriveWithNothingTyped) {
            // Required by timed and race modes: a run has to be able to end
            // while the typist stares at the screen.
            testing::SpyMode mode;
            testing::ModeDriver driver{"ab", mode};

            driver.tick(20);

            EXPECT_EQ(mode.ticks, 20U);
            EXPECT_EQ(mode.keystrokes, 0U);
            EXPECT_EQ(mode.last_tick, Millis{10'000});
        }

        TEST(ModeInterfaceTest, AModeSeesTheModelBehindTheKeystroke) {
            testing::SpyMode mode;
            testing::ModeDriver driver{"ab", mode};

            driver.type("ab");

            EXPECT_EQ(mode.last_cursor, GraphemeIndex{2}) << "the keystroke has already been applied";
        }

        TEST(ModeInterfaceTest, FinishedStaysFinished) {
            // The contract every mode inherits: once a run is over it stays
            // over. A mode that flickered back would restart a run the user has
            // already been shown the results of.
            testing::SpyMode mode{"spy", 3};
            testing::ModeDriver driver{"abcdef", mode};

            driver.tick(2);
            ASSERT_FALSE(mode.is_finished());

            driver.tick();
            ASSERT_TRUE(mode.is_finished());

            for (int i = 0; i < 20; ++i) {
                driver.tick();
                driver.type("a");
                ASSERT_TRUE(mode.is_finished()) << "after tick " << i;
            }
        }

        TEST(ModeInterfaceTest, ProgressCarriesOnlyWhatTheModeActuallyKnows) {
            // The variant is the point: a mode with no end reports elapsed time
            // and nothing else, and the HUD cannot read a remaining-seconds
            // field it never filled in.
            testing::SpyMode mode;
            testing::ModeDriver driver{"ab", mode};
            driver.tick(4);

            const ModeProgress progress = mode.progress();
            ASSERT_TRUE(std::holds_alternative<OpenProgress>(progress));
            EXPECT_EQ(std::get<OpenProgress>(progress).elapsed, Millis{2'000});
        }

        // The registry.

        std::unique_ptr<IMode> spy(std::string_view id) { return std::make_unique<testing::SpyMode>(id); }

        TEST(ModeRegistryTest, CreatesTheModeRegisteredUnderAnId) {
            ModeRegistry registry;
            registry.register_mode("timed", [] { return spy("timed"); });
            registry.register_mode("quote", [] { return spy("quote"); });

            const Result<std::unique_ptr<IMode>> timed = registry.create("timed");
            const Result<std::unique_ptr<IMode>> quote = registry.create("quote");

            ASSERT_TRUE(timed);
            ASSERT_TRUE(quote);
            EXPECT_EQ((*timed)->id(), "timed");
            EXPECT_EQ((*quote)->id(), "quote");
        }

        TEST(ModeRegistryTest, AnUnknownIdIsAnErrorThatNamesIt) {
            ModeRegistry registry;
            registry.register_mode("timed", [] { return spy("timed"); });

            const Result<std::unique_ptr<IMode>> unknown = registry.create("tiemd");

            ASSERT_FALSE(unknown);
            EXPECT_EQ(unknown.error().code, ErrorCode::UnknownMode);
            EXPECT_EQ(unknown.error().context, "tiemd") << "a typo in a config file deserves to be quoted back";
        }

        TEST(ModeRegistryTest, EveryCallGivesAFreshMode) {
            ModeRegistry registry;
            registry.register_mode("spy", [] { return spy("spy"); });

            const Result<std::unique_ptr<IMode>> first = registry.create("spy");
            const Result<std::unique_ptr<IMode>> second = registry.create("spy");

            ASSERT_TRUE(first);
            ASSERT_TRUE(second);
            EXPECT_NE(first->get(), second->get()) << "a second run must not inherit the first one's state";
        }

        TEST(ModeRegistryTest, RegisteringAnIdTwiceReplacesIt) {
            ModeRegistry registry;
            registry.register_mode("timed", [] { return spy("first"); });
            registry.register_mode("timed", [] { return spy("second"); });

            const Result<std::unique_ptr<IMode>> mode = registry.create("timed");

            ASSERT_TRUE(mode);
            EXPECT_EQ((*mode)->id(), "second");
            EXPECT_EQ(registry.size(), 1U);
        }

        TEST(ModeRegistryTest, IdsComeBackSorted) {
            ModeRegistry registry;
            registry.register_mode("zen", [] { return spy("zen"); });
            registry.register_mode("timed", [] { return spy("timed"); });
            registry.register_mode("quote", [] { return spy("quote"); });

            EXPECT_EQ(registry.ids(), (std::vector<std::string>{"quote", "timed", "zen"}));
            EXPECT_TRUE(registry.contains("zen"));
            EXPECT_FALSE(registry.contains("race"));
        }

        TEST(ModeRegistryTest, AnEmptyRegistryKnowsNothing) {
            const ModeRegistry registry;

            EXPECT_EQ(registry.size(), 0U);
            EXPECT_TRUE(registry.ids().empty());
            EXPECT_FALSE(registry.create("timed"));
        }

    }  // namespace
}  // namespace typeit::core
