#pragma once

#include <memory>
#include <string_view>

#include "comm/can_bus.hpp"
#include "comm/can_frame.hpp"
#include "fakes/fake_clock.hpp"

namespace cvc::test {

class ICanBusHarness {
public:
    virtual ~ICanBusHarness() = default;

    [[nodiscard]]
    virtual comm::ICanBus& bus() = 0;
    [[nodiscard]]
    virtual std::string_view interfaceName() const = 0;
    [[nodiscard]]
    virtual std::string_view missingInterfaceName() const = 0;
    [[nodiscard]]
    virtual CFakeClock& clock() = 0;

    virtual void deliver(const comm::SCanFrame& frame) = 0;
    [[nodiscard]]
    virtual bool takeTransmitted(comm::SCanFrame& frame) = 0;
};

using TCanBusHarnessFactory = std::unique_ptr<ICanBusHarness> (*)();

}  // namespace cvc::test
