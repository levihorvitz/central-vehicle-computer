#include <linux/can.h>
#include <net/if.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>

#include <gtest/gtest.h>

#include "comm/can_bus.hpp"
#include "comm/can_frame.hpp"
#include "comm/socket_can_bus.hpp"
#include "fakes/fake_clock.hpp"
#include "support/can_bus_contract.hpp"
#include "support/can_bus_harness.hpp"
#include "support/can_test_utils.hpp"

namespace cvc::test {
namespace {

using comm::CSocketCanBus;
using comm::EBusMode;
using comm::EOpenResult;
using comm::EReceiveResult;
using comm::SCanFrame;
using comm::SReceivedFrame;

constexpr const char* kInterface = "vcan0";
constexpr core::TDuration kFrameTimeout = std::chrono::milliseconds(100);
constexpr std::chrono::milliseconds kPeerTimeout{50};

[[noreturn]]
void fail(const std::string& what) {
    throw std::runtime_error(what + ": " + std::strerror(errno) + " (is vcan0 up? see scripts/setup_vcan.sh)");
}

can_frame toKernelFrame(const SCanFrame& frame) {
    can_frame kernelFrame{};
    kernelFrame.can_id = frame.id;
    kernelFrame.len = frame.dlc;
    for (std::size_t i = 0U; i < frame.dlc && i < sizeof(kernelFrame.data); ++i) {
        kernelFrame.data[i] = frame.data[i];
    }
    return kernelFrame;
}

SCanFrame fromKernelFrame(const can_frame& kernelFrame) {
    SCanFrame frame;
    frame.id = kernelFrame.can_id & CAN_SFF_MASK;
    frame.dlc = kernelFrame.len;
    for (std::size_t i = 0U; i < frame.dlc && i < frame.data.size(); ++i) {
        frame.data[i] = kernelFrame.data[i];
    }
    return frame;
}

// A second raw socket on the same interface: plays the rest of the CAN network.
class CPeerSocket final {
public:
    explicit CPeerSocket(const char* interfaceName) {
        fd_ = ::socket(PF_CAN, SOCK_RAW, CAN_RAW);
        if (fd_ < 0) {
            fail("peer socket");
        }
        sockaddr_can address{};
        address.can_family = AF_CAN;
        address.can_ifindex = static_cast<int>(::if_nametoindex(interfaceName));
        if (address.can_ifindex == 0) {
            ::close(fd_);
            fail(std::string("if_nametoindex(") + interfaceName + ")");
        }
        if (::bind(fd_, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) < 0) {
            ::close(fd_);
            fail("peer bind");
        }
    }

    CPeerSocket(const CPeerSocket&) = delete;
    CPeerSocket& operator=(const CPeerSocket&) = delete;
    CPeerSocket(CPeerSocket&&) = delete;
    CPeerSocket& operator=(CPeerSocket&&) = delete;

    ~CPeerSocket() { ::close(fd_); }

    void send(const can_frame& frame) {
        if (::write(fd_, &frame, sizeof(frame)) != static_cast<ssize_t>(sizeof(frame))) {
            fail("peer write");
        }
    }

    [[nodiscard]]
    bool receive(can_frame& frame, std::chrono::milliseconds timeout) {
        pollfd waitFor{fd_, POLLIN, 0};
        if (::poll(&waitFor, 1, static_cast<int>(timeout.count())) <= 0) {
            return false;
        }
        return ::read(fd_, &frame, sizeof(frame)) == static_cast<ssize_t>(sizeof(frame));
    }

private:
    int fd_ = -1;
};

class CSocketCanBusHarness final : public ICanBusHarness {
public:
    CSocketCanBusHarness() : bus_(clock_), peer_(kInterface) {}

    [[nodiscard]]
    comm::ICanBus& bus() override {
        return bus_;
    }
    [[nodiscard]]
    std::string_view interfaceName() const override {
        return kInterface;
    }
    [[nodiscard]]
    std::string_view missingInterfaceName() const override {
        return "cvcmissing0";
    }
    [[nodiscard]]
    CFakeClock& clock() override {
        return clock_;
    }

    void deliver(const SCanFrame& frame) override { peer_.send(toKernelFrame(frame)); }

    [[nodiscard]]
    bool takeTransmitted(SCanFrame& frame) override {
        can_frame kernelFrame{};
        if (!peer_.receive(kernelFrame, kPeerTimeout)) {
            return false;
        }
        frame = fromKernelFrame(kernelFrame);
        return true;
    }

private:
    CFakeClock clock_;
    CSocketCanBus bus_;
    CPeerSocket peer_;
};

std::unique_ptr<ICanBusHarness> makeSocketCanBusHarness() {
    return std::make_unique<CSocketCanBusHarness>();
}

class CSocketCanBusTest : public ::testing::Test {
protected:
    void SetUp() override { ASSERT_EQ(bus_.open(kInterface, EBusMode::kSimulation), EOpenResult::kOpened); }

    CFakeClock clock_;
    CSocketCanBus bus_{clock_};
    CPeerSocket peer_{kInterface};
};

}  // namespace

INSTANTIATE_TEST_SUITE_P(SocketCan, CCanBusContractTest, ::testing::Values(&makeSocketCanBusHarness));

// --- Behaviour that only a real socket can produce ---

// SW_CAN_002
TEST_F(CSocketCanBusTest, DoesNotDeliverExtendedIdFrames) {
    can_frame extended = toKernelFrame(makeFrame(0x100U));
    extended.can_id |= CAN_EFF_FLAG;
    peer_.send(extended);
    peer_.send(toKernelFrame(makeFrame(0x101U)));

    SReceivedFrame received;
    ASSERT_EQ(bus_.receive(received, kFrameTimeout), EReceiveResult::kReceived);
    EXPECT_EQ(received.frame.id, 0x101U);
}

// SW_CAN_011
TEST_F(CSocketCanBusTest, DoesNotDeliverRemoteFrames) {
    can_frame remote = toKernelFrame(makeFrame(0x100U, 0U));
    remote.can_id |= CAN_RTR_FLAG;
    peer_.send(remote);
    peer_.send(toKernelFrame(makeFrame(0x102U)));

    SReceivedFrame received;
    ASSERT_EQ(bus_.receive(received, kFrameTimeout), EReceiveResult::kReceived);
    EXPECT_EQ(received.frame.id, 0x102U);
}

// SW_CAN_011
TEST_F(CSocketCanBusTest, DoesNotDeliverErrorFrames) {
    can_frame error = toKernelFrame(makeFrame(0x000U));
    error.can_id = CAN_ERR_FLAG;
    peer_.send(error);
    peer_.send(toKernelFrame(makeFrame(0x103U)));

    SReceivedFrame received;
    ASSERT_EQ(bus_.receive(received, kFrameTimeout), EReceiveResult::kReceived);
    EXPECT_EQ(received.frame.id, 0x103U);
}

// SW_CAN_008
TEST_F(CSocketCanBusTest, ReturnsFrameThatArrivesDuringTheWait) {
    std::thread sender([this] {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        peer_.send(toKernelFrame(makeFrame(0x104U)));
    });

    const auto start = std::chrono::steady_clock::now();
    SReceivedFrame received;
    const EReceiveResult result = bus_.receive(received, std::chrono::seconds(2));
    const auto elapsed = std::chrono::steady_clock::now() - start;
    sender.join();

    EXPECT_EQ(result, EReceiveResult::kReceived);
    EXPECT_EQ(received.frame.id, 0x104U);
    EXPECT_TRUE(elapsed < std::chrono::seconds(1));
}

// SW_CAN_008
TEST_F(CSocketCanBusTest, WaitsAtLeastTheTimeoutWhenNoFrameArrives) {
    constexpr auto kTimeout = std::chrono::milliseconds(50);

    const auto start = std::chrono::steady_clock::now();
    SReceivedFrame received;
    const EReceiveResult result = bus_.receive(received, kTimeout);
    const auto elapsed = std::chrono::steady_clock::now() - start;

    EXPECT_EQ(result, EReceiveResult::kTimeout);
    EXPECT_TRUE(elapsed >= kTimeout);
}

// SW_CAN_013
TEST_F(CSocketCanBusTest, CountsFramesDroppedByAFullReceiveBuffer) {
    constexpr std::size_t kFlood = 20000U;
    for (std::size_t i = 0U; i < kFlood; ++i) {
        peer_.send(toKernelFrame(makeFrame(0x105U)));
    }

    SReceivedFrame received;
    EReceiveResult result = EReceiveResult::kReceived;
    while (result == EReceiveResult::kReceived) {
        result = bus_.receive(received, kFrameTimeout);
    }
    ASSERT_EQ(result, EReceiveResult::kTimeout);

    peer_.send(toKernelFrame(makeFrame(0x106U)));
    ASSERT_EQ(bus_.receive(received, kFrameTimeout), EReceiveResult::kReceived);
    EXPECT_EQ(received.frame.id, 0x106U);
    EXPECT_TRUE(bus_.droppedFrames() > 0U);
}

// SW_CAN_017. Manual: bring vcan0 down while this test waits, e.g.
// (sleep 2 && sudo ip link set vcan0 down) & ./socketcan_tests --gtest_also_run_disabled_tests ...
TEST_F(CSocketCanBusTest, DISABLED_ReportsInterfaceLostWhenInterfaceGoesDown) {
    constexpr core::TDuration kManualWindow = std::chrono::seconds(30);

    SReceivedFrame received;
    EXPECT_EQ(bus_.receive(received, kManualWindow), EReceiveResult::kInterfaceLost);
    EXPECT_EQ(bus_.send(makeFrame(0x123U)), comm::ESendResult::kInterfaceLost);
}

// SW_CAN_018, SW_CANRX_011. Manual: bring vcan0 down while this test waits, e.g.
// (sleep 2 && sudo ip link set vcan0 down) & ./socketcan_tests --gtest_also_run_disabled_tests ...
TEST_F(CSocketCanBusTest, DISABLED_ReceiveReportsInterfaceLostAfterSendReportedIt) {
    constexpr core::TDuration kManualWindow = std::chrono::seconds(30);
    constexpr core::TDuration kRetryPeriod = std::chrono::milliseconds(10);
    constexpr core::TDuration kReceiveTimeout = std::chrono::milliseconds(100);

    const auto deadline = std::chrono::steady_clock::now() + kManualWindow;
    comm::ESendResult sendResult = bus_.send(makeFrame(0x123U));
    while (sendResult != comm::ESendResult::kInterfaceLost && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(kRetryPeriod);
        sendResult = bus_.send(makeFrame(0x123U));
    }
    ASSERT_EQ(sendResult, comm::ESendResult::kInterfaceLost);

    SReceivedFrame received;
    EXPECT_EQ(bus_.receive(received, kReceiveTimeout), EReceiveResult::kInterfaceLost);
}

}  // namespace cvc::test
