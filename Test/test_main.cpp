#include <gtest/gtest.h>
#include "../../src/IO/LogHelper.h"

int main(int argc, char **argv)
{
    LogData::Initialize();
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
