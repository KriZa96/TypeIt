//
// Created by kriza on 31/03/2025.
//
#include <gtest/gtest.h>

#include "../include/engines/InputAccuracyEngine.h"


class InputAccuracyTest : public ::testing::Test {
protected:
    InputAccuracyTest(): input_accuracy() {};
    InputAccuracyEngine input_accuracy;
};


TEST_F(InputAccuracyTest, PercentageOfCorrectInput) {
    for (int i=0; i < 100; ++i) {
        input_accuracy.push_character_accuracy(true);
    }

    EXPECT_EQ(input_accuracy.get_percentage_of_correct_input(), 100.);
}

TEST_F(InputAccuracyTest, PercentageOfCorrectInputOnStart) {
    EXPECT_EQ(input_accuracy.get_percentage_of_correct_input(), 0.);
}