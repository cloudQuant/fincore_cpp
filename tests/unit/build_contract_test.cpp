#include <gtest/gtest.h>
#include <fincore/version.hpp>

// Pins the native library version only; the sibling `pinned_version_*` constants
// carry a different version on purpose and are asserted by no case in this file.
TEST(BuildContract, PublishesSemanticVersion) {
    EXPECT_EQ(fincore::version_major, 0);
    EXPECT_EQ(fincore::version_minor, 1);
    EXPECT_EQ(fincore::version_patch, 0);
}
