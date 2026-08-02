#include <gtest/gtest.h>
#include <string>
#include <utility>

#include "typeit/core/util/Result.h"

namespace typeit::core {
    namespace {

        Result<int> parse_positive(int value) {
            if (value <= 0) {
                return fail(ErrorCode::ConfigInvalid, "value=" + std::to_string(value));
            }
            return value;
        }

        Result<int> doubled(int value) {
            const Result<int> parsed = parse_positive(value);
            if (!parsed) {
                return std::unexpected{parsed.error()};
            }
            return *parsed * 2;
        }

        Result<int> quadrupled(int value) {
            const Result<int> inner = doubled(value);
            if (!inner) {
                return std::unexpected{inner.error()};
            }
            return *inner * 2;
        }

        Status require_readable(bool readable) {
            if (!readable) {
                return fail(ErrorCode::FileUnreadable, "/etc/shadow");
            }
            return {};
        }

        Status open_and_read(bool readable) {
            if (const Status opened = require_readable(readable); !opened) {
                return opened;
            }
            return {};
        }

        TEST(ResultTest, CarriesAValueOrAnError) {
            const Result<int> ok = parse_positive(7);
            ASSERT_TRUE(ok);
            EXPECT_EQ(*ok, 7);

            const Result<int> bad = parse_positive(0);
            ASSERT_FALSE(bad);
            EXPECT_EQ(bad.error().code, ErrorCode::ConfigInvalid);
        }

        TEST(ResultTest, ContextSurvivesPropagationAcrossThreeLevels) {
            const Result<int> propagated = quadrupled(-3);

            ASSERT_FALSE(propagated);
            EXPECT_EQ(propagated.error().code, ErrorCode::ConfigInvalid);
            EXPECT_EQ(propagated.error().context, "value=-3");
            EXPECT_EQ(propagated.error().message, default_message(ErrorCode::ConfigInvalid));
        }

        TEST(ResultTest, ContextSurvivesAMove) {
            Error original = make_error(ErrorCode::DbQuery, "SELECT 1");
            const Error moved = std::move(original);

            EXPECT_EQ(moved.code, ErrorCode::DbQuery);
            EXPECT_EQ(moved.context, "SELECT 1");
            EXPECT_FALSE(moved.message.empty());
        }

        TEST(ResultTest, StatusShortCircuits) {
            EXPECT_TRUE(open_and_read(true));

            const Status failed = open_and_read(false);
            ASSERT_FALSE(failed);
            EXPECT_EQ(failed.error().code, ErrorCode::FileUnreadable);
            EXPECT_EQ(failed.error().context, "/etc/shadow");
        }

        TEST(ResultTest, EveryCodeHasANonEmptyDefaultMessage) {
            for (const ErrorCode code: kAllErrorCodes) {
                EXPECT_FALSE(default_message(code).empty()) << "code " << static_cast<int>(code);
            }
        }

        TEST(ResultTest, EveryDefaultMessageIsDistinct) {
            // A shared message means two failures are indistinguishable to the person
            // reading them, which is the same as having no message.
            for (std::size_t i = 0; i < kAllErrorCodes.size(); ++i) {
                for (std::size_t j = i + 1; j < kAllErrorCodes.size(); ++j) {
                    EXPECT_NE(default_message(kAllErrorCodes[i]), default_message(kAllErrorCodes[j]))
                            << "codes " << i << " and " << j;
                }
            }
        }

        TEST(ResultTest, FormattingAppendsContextOnlyWhenThereIsSome) {
            EXPECT_EQ(to_string(make_error(ErrorCode::EmptyText)), default_message(ErrorCode::EmptyText));
            EXPECT_EQ(to_string(make_error(ErrorCode::FileNotFound, "/tmp/missing.txt")),
                      std::string{default_message(ErrorCode::FileNotFound)} + " (/tmp/missing.txt)");
        }

        TEST(ResultTest, ErrorsCompareByValue) {
            EXPECT_EQ(make_error(ErrorCode::DbOpen, "a"), make_error(ErrorCode::DbOpen, "a"));
            EXPECT_NE(make_error(ErrorCode::DbOpen, "a"), make_error(ErrorCode::DbOpen, "b"));
            EXPECT_NE(make_error(ErrorCode::DbOpen), make_error(ErrorCode::DbQuery));
        }

    }  // namespace
}  // namespace typeit::core
