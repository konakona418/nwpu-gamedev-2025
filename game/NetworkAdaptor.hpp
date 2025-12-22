#pragma once

#include "Core/Common.hpp"
#include "Core/Meta/Feature.hpp"

#include <concurrentqueue.h>
#include <enet/enet.h>

namespace game {
    using TransmitQueue = moodycamel::ConcurrentQueue<moe::Vector<uint8_t>>;

    struct NetworkAdaptor : public moe::Meta::NonCopyable<NetworkAdaptor> {
    public:
        static constexpr uint32_t MAGIC = 0x00769394;
        static constexpr uint32_t MAX_CHANNELS = 2;

        void init(moe::StringView serverAddress, uint16_t port);

        void shutdown();

        moe::SharedPtr<TransmitQueue> getSendQueue() {
            return m_sendQueue;
        }

        moe::SharedPtr<TransmitQueue> getReceiveQueue() {
            return m_receiveQueue;
        }

    private:
        ENetPeer* m_serverPeer{nullptr};
        ENetHost* m_client{nullptr};

        moe::String m_serverAddress;
        uint16_t m_serverPort{0};

        std::atomic_bool m_running{false};
        moe::UniquePtr<std::thread> m_networkThread;

        moe::SharedPtr<TransmitQueue> m_sendQueue = std::make_shared<TransmitQueue>();
        moe::SharedPtr<TransmitQueue> m_receiveQueue = std::make_shared<TransmitQueue>();

        void launchNetworkThread();
        void networkMain();

        bool tryConnectToServer();

        void handleEnetEvent(const ENetEvent& event);
    };
}// namespace game