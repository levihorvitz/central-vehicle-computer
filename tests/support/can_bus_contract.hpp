#pragma once

#include <memory>

#include <gtest/gtest.h>

#include "comm/can_bus.hpp"
#include "support/can_bus_harness.hpp"

namespace cvc::test {

class CCanBusContractTest : public ::testing::TestWithParam<TCanBusHarnessFactory> {
protected:
    void SetUp() override { harness_ = GetParam()(); }

    [[nodiscard]]
    comm::ICanBus& bus() {
        return harness_->bus();
    }

    [[nodiscard]]
    bool openBus(comm::EBusMode mode = comm::EBusMode::kSimulation) {
        return bus().open(harness_->interfaceName(), mode) == comm::EOpenResult::kOpened;
    }

    [[nodiscard]]
    bool nothingTransmitted() {
        comm::SCanFrame frame;
        return !harness_->takeTransmitted(frame);
    }

    std::unique_ptr<ICanBusHarness> harness_;
};

}  // namespace cvc::test
