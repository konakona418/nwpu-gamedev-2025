#pragma once

#include "Core/Common.hpp"
#include "Core/Meta/Feature.hpp"

#include <concurrentqueue.h>
#include <enet/enet.h>

namespace game {
    struct TransmitSend {
        moe::Vector<uint8_t> payload;
        bool reliable;
    };

    struct TransmitRecv {
        moe::Vector<uint8_t> payload;
    };

    using TransmitQueueRecv = moodycamel::ConcurrentQueue<TransmitRecv>;
    using TransmitQueueSend = moodycamel::ConcurrentQueue<TransmitSend>;

    struct NetworkAdaptor : public moe::Meta::NonCopyable<NetworkAdaptor> {
    public:
        static constexpr uint32_t MAGIC = 0x00769394;
        static constexpr uint32_t MAX_CHANNELS = 2;

        void init(moe::StringView serverAddress, uint16_t port);

        void shutdown();

        void sendData(moe::Span<const uint8_t> data, bool reliable);

        moe::Optional<TransmitRecv> tryReceiveData();

    private:
        // wait for 1ms
        static constexpr uint32_t NETWORK_LOOP_TIME_WAIT_MS = 1;
        static constexpr size_t MAX_SEND_REQUEST_PER_CYCLE = 32;

        struct Channels {
            static constexpr uint8_t RELIABLE = 0;
            static constexpr uint8_t UNRELIABLE = 1;
        };

        ENetPeer* m_serverPeer{nullptr};
        ENetHost* m_client{nullptr};

        moe::String m_serverAddress;
        uint16_t m_serverPort{0};

        std::atomic_bool m_running{false};
        moe::UniquePtr<std::thread> m_networkThread;

        moe::UniquePtr<TransmitQueueSend> m_sendQueue = std::make_unique<TransmitQueueSend>();
        moe::UniquePtr<TransmitQueueRecv> m_receiveQueue = std::make_unique<TransmitQueueRecv>();

        void launchNetworkThread();
        void networkMain();

        bool tryConnectToServer();

        void handleEnetEvent(const ENetEvent& event);
        void handleEnetSendRequest();
    };
}// namespace game