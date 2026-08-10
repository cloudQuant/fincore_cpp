#include <gtest/gtest.h>
#include <fincore/version.hpp>

TEST(BuildContract, PublishesSemanticVersion) {
    EXPECT_EQ(fincore::version_major, 0);
    EXPECT_EQ(fincore::version_minor, 1);
    EXPECT_EQ(fincore::version_patch, 0);
}
