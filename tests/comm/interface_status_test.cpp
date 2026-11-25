#include <gtest/gtest.h>

#include "comm/interface_status.hpp"

namespace cvc::test {

using comm::CInterfaceStatus;
using comm::EInterfaceStatus;

// SW_CANRX_009
TEST(InterfaceStatusTest, StartsAvailable) {
    const CInterfaceStatus status;
    EXPECT_EQ(status.get(), EInterfaceStatus::kAvailable);
}

// SW_CANRX_009
TEST(InterfaceStatusTest, ReportsLostAfterMarkLost) {
    CInterfaceStatus status;
    status.markLost();
    EXPECT_EQ(status.get(), EInterfaceStatus::kLost);
}

// SW_CANRX_012
TEST(InterfaceStatusTest, StaysLostWhenMarkedLostAgain) {
    CInterfaceStatus status;
    status.markLost();
    status.markLost();
    EXPECT_EQ(status.get(), EInterfaceStatus::kLost);
}

}  // namespace cvc::test
