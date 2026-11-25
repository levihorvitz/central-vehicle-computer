#include <exception>
#include <thread>

#include "comm/can_rx_thread.hpp"

namespace cvc::comm {

CCanRxThread::CCanRxThread(ICanBus& bus, TCanRxQueue& queue, CInterfaceStatus& status) noexcept
    : bus_(bus), queue_(queue), status_(status) {}

CCanRxThread::~CCanRxThread() {
    stop();
}

bool CCanRxThread::start() {
    if (running_.load(std::memory_order_acquire)) {
        return false;
    }
    if (thread_.joinable()) {
        thread_.join();
    }

    stopRequested_.store(false, std::memory_order_relaxed);
    running_.store(true, std::memory_order_release);
    try {
        thread_ = std::thread(&CCanRxThread::run, this);
    } catch (const std::exception&) {
        running_.store(false, std::memory_order_release);
        return false;
    }
    return true;
}

void CCanRxThread::stop() noexcept {
    stopRequested_.store(true, std::memory_order_relaxed);
    if (thread_.joinable()) {
        thread_.join();
    }
}

bool CCanRxThread::isRunning() const noexcept {
    return running_.load(std::memory_order_acquire);
}

std::uint64_t CCanRxThread::discardedFrames() const noexcept {
    return discarded_.load(std::memory_order_relaxed);
}

void CCanRxThread::run() noexcept {
    SReceivedFrame received{};
    bool keepReceiving = true;
    while (keepReceiving && !stopRequested_.load(std::memory_order_relaxed)) {
        switch (bus_.receive(received, kReceiveTimeout)) {
            case EReceiveResult::kReceived:
                if (!queue_.tryPush(received)) {
                    discarded_.fetch_add(1U, std::memory_order_relaxed);
                }
                break;
            case EReceiveResult::kTimeout:
                break;
            case EReceiveResult::kInterfaceLost:
                status_.markLost();
                keepReceiving = false;
                break;
            case EReceiveResult::kNotOpen:
                keepReceiving = false;
                break;
        }
    }
    running_.store(false, std::memory_order_release);
}

}  // namespace cvc::comm
