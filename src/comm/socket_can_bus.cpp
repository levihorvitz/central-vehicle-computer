#include <cstring>
#include <algorithm>
#include <unistd.h>
#include <linux/can.h>
#include <net/if.h>
#include <sys/socket.h>
#include <cstddef>
#include <cerrno>
#include <poll.h>
#include <sys/uio.h>
#include <chrono>
#include <limits>

#include "comm/socket_can_bus.hpp"


namespace cvc::comm {

    namespace {

    constexpr std::uint32_t kObdFunctionalRequestId = 0x7DFU;
    constexpr std::uint32_t kObdFirstPhysicalRequestId = 0x7E0U;
    constexpr std::uint32_t kObdLastPhysicalRequestId = 0x7E7U;

    using TSteadyClock = std::chrono::steady_clock;

    [[nodiscard]] bool isObdRequestId(std::uint32_t id) noexcept {
        return id == kObdFunctionalRequestId ||
            (kObdFirstPhysicalRequestId<=id && id <= kObdLastPhysicalRequestId);
    }

    }  // namespace

CSocketCanBus::CSocketCanBus(core::IClock& clock) : clock_(clock) {}

CSocketCanBus::~CSocketCanBus() {
    close();
}

EOpenResult CSocketCanBus::open(std::string_view interfaceName, EBusMode mode) {
    if (fd_ != -1) {
        return EOpenResult::kAlreadyOpen;
    }

    if (interfaceName.empty() || interfaceName.size() >= static_cast<std::size_t>(IFNAMSIZ)) {
        return EOpenResult::kFailed;
    }

    char name[IFNAMSIZ]{};
    for (std::size_t i = 0U; i < interfaceName.size(); ++i) {
        name[i] = interfaceName[i];
    }

    const int fd = ::socket(PF_CAN, SOCK_RAW | SOCK_CLOEXEC, CAN_RAW);
    if (fd == -1) {
        return EOpenResult::kFailed;
    }

    const auto fail = [fd]() noexcept {
        ::close(fd);
        return EOpenResult::kFailed;
    };

    const unsigned int index = ::if_nametoindex(name);
    if (index == 0U) {
        return fail();
    }

    sockaddr_can address{};
    address.can_family = static_cast<sa_family_t>(AF_CAN);
    address.can_ifindex = static_cast<int>(index);
    if (::bind(fd, reinterpret_cast<const sockaddr*>(&address), static_cast<socklen_t>(sizeof(address))) != 0) {
        return fail();
    }

    const int enable = 1;
    //enables a socket option that allows you to receive information about dropped packets.
    if (::setsockopt(fd, SOL_SOCKET, SO_RXQ_OVFL, &enable, static_cast<socklen_t>(sizeof(enable))) != 0) {
        return fail();
    }

    fd_ = fd;
    mode_ = mode;
    dropped_.store(0U);
    return EOpenResult::kOpened;
}

void CSocketCanBus::close() noexcept {
    if (fd_==-1)
        return;

    ::close(fd_);
    fd_=-1;
}

ESendResult CSocketCanBus::send(const SCanFrame& frame) {
    if (fd_ == -1) {
        return ESendResult::kNotOpen;
    }

    if (frame.id > kMaxStandardId || frame.dlc > kMaxCanDlc) {
        return ESendResult::kInvalidFrame;
    }

    if (mode_ == EBusMode::kRealVehicle && !isObdRequestId(frame.id)) {
        return ESendResult::kNotPermitted;
    }

    can_frame kernelFrame{};
    kernelFrame.can_id = frame.id;
    kernelFrame.len = frame.dlc;
    for (std::size_t i = 0U; i < static_cast<std::size_t>(frame.dlc); ++i) {
        kernelFrame.data[i] = frame.data[i];
    }

    const ssize_t sent = ::send(fd_, &kernelFrame, sizeof(kernelFrame), MSG_DONTWAIT);
    if (sent == static_cast<ssize_t>(sizeof(kernelFrame))) {
        return ESendResult::kAccepted;
    }

    if (sent == -1) {
        const int error = errno;
        if (error == EAGAIN || error == ENOBUFS) {
            return ESendResult::kBufferFull;
        }
    }

    return ESendResult::kInterfaceLost;
}

EReceiveResult CSocketCanBus::receive(SReceivedFrame& received, core::TDuration timeout) {
    if (fd_ == -1) {
        return EReceiveResult::kNotOpen;
    }

    const TSteadyClock::time_point deadline =
        TSteadyClock::now() + std::chrono::duration_cast<TSteadyClock::duration>(timeout);

    while (true){
        const TSteadyClock::duration remaining =
            std::max(deadline - TSteadyClock::now(), TSteadyClock::duration::zero());
        const std::chrono::milliseconds remainingMs = std::chrono::ceil<std::chrono::milliseconds>(remaining);
        const int pollTimeout = static_cast<int>(std::min<std::chrono::milliseconds::rep>(
            remainingMs.count(), std::numeric_limits<int>::max()));

        pollfd pollEntry{fd_, POLLIN, 0};
        const int ready = ::poll(&pollEntry, 1U, pollTimeout);
        if (ready == 0) {
            return EReceiveResult::kTimeout;
        }
        if (ready == -1) {
            if (errno == EINTR) {
                continue;
            }
            return EReceiveResult::kInterfaceLost;
        }
        if ((pollEntry.revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
            return EReceiveResult::kInterfaceLost;
        }

        can_frame kernelFrame{};
        iovec ioVector{&kernelFrame, sizeof(kernelFrame)};
        alignas(cmsghdr) char control[CMSG_SPACE(sizeof(std::uint32_t))]{};

        msghdr message{};
        message.msg_iov = &ioVector;
        message.msg_iovlen = 1U;
        message.msg_control = control;
        message.msg_controllen = sizeof(control);

        const ssize_t bytes = ::recvmsg(fd_, &message, MSG_DONTWAIT);
        if (bytes == -1) {
            if (errno == EAGAIN || errno == EINTR) {
                continue;
            }
            return EReceiveResult::kInterfaceLost;
        }
        if (bytes != static_cast<ssize_t>(sizeof(kernelFrame))) {
            continue;
        }

        for (cmsghdr* header = CMSG_FIRSTHDR(&message); header != nullptr;
             header = CMSG_NXTHDR(&message, header)) {
            if (header->cmsg_level == SOL_SOCKET && header->cmsg_type == SO_RXQ_OVFL) {
                std::uint32_t droppedCount = 0U;
                std::memcpy(&droppedCount, CMSG_DATA(header), sizeof(droppedCount));
                dropped_.store(droppedCount);
            }
        }

        if ((kernelFrame.can_id & (CAN_EFF_FLAG | CAN_RTR_FLAG | CAN_ERR_FLAG)) != 0U ||
            kernelFrame.len > kMaxCanDlc) {
            continue;
        }

        SCanFrame frame{};
        frame.id = kernelFrame.can_id;
        frame.dlc = kernelFrame.len;
        for (std::size_t i = 0U; i < static_cast<std::size_t>(kernelFrame.len); ++i) {
            frame.data[i] = kernelFrame.data[i];
        }

        received.frame = frame;
        received.timestamp = clock_.now();
        return EReceiveResult::kReceived;
    }
}

std::uint64_t CSocketCanBus::droppedFrames() const noexcept {
    return dropped_.load();
}

}  // namespace cvc::comm
