#include <gtest/gtest.h>

#include <regex>
#include <string>

#include "typeit/core/Version.h"


TEST(VersionTest, VersionStringHasSemVerShape) {
    const std::regex semver(R"(^\d+\.\d+\.\d+(-[0-9A-Za-z.-]+)?$)");

    EXPECT_TRUE(std::regex_match(typeit::kVersionString, semver)) << typeit::kVersionString;
}


TEST(VersionTest, ComponentsMatchTheVersionString) {
    const std::string expected = std::to_string(typeit::kVersionMajor) + "." + std::to_string(typeit::kVersionMinor) +
                                 "." + std::to_string(typeit::kVersionPatch);
    const std::string actual(typeit::kVersionString);

    EXPECT_EQ(actual.substr(0, expected.size()), expected);
    EXPECT_TRUE(actual.size() == expected.size() || actual[expected.size()] == '-') << actual;
}


TEST(VersionTest, EmptyPrereleaseLeavesNoTrailingHyphen) {
    const std::string version(typeit::kVersionString);
    const std::string prerelease(typeit::kVersionPrerelease);

    EXPECT_NE(version.back(), '-');
    if (prerelease.empty()) {
        EXPECT_EQ(version.find('-'), std::string::npos) << version;
    } else {
        EXPECT_EQ(version, version.substr(0, version.size() - prerelease.size() - 1) + "-" + prerelease);
    }
}
