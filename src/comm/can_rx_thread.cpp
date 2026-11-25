#include "comm/can_rx_thread.hpp"

namespace cvc::comm {

CCanRxThread::CCanRxThread(ICanBus& bus, TCanRxQueue& queue, CInterfaceStatus& status) noexcept
    : bus_(bus), queue_(queue), status_(status) {}

CCanRxThread::~CCanRxThread() {
    stop();
}

bool CCanRxThread::start() {
    return false;
}

void CCanRxThread::stop() noexcept {}

bool CCanRxThread::isRunning() const noexcept {
    return false;
}

std::uint64_t CCanRxThread::discardedFrames() const noexcept {
    return 0U;
}

void CCanRxThread::run() noexcept {}

}  // namespace cvc::comm
