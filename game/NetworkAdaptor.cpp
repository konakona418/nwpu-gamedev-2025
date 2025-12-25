#include "NetworkAdaptor.hpp"

#include <enet/types.h>

namespace game {
    void NetworkAdaptor::init(moe::StringView serverAddress, uint16_t port) {
        m_serverAddress = serverAddress;
        m_serverPort = port;

        launchNetworkThread();
    }

    void NetworkAdaptor::shutdown() {
        moe::Logger::info("Stopping network thread");

        m_running = false;
        if (m_networkThread && m_networkThread->joinable()) {
            m_networkThread->join();
        }

        moe::Logger::info("Network thread stopped");
    }

    void NetworkAdaptor::launchNetworkThread() {
        m_running = true;
        m_networkThread = std::make_unique<std::thread>(&NetworkAdaptor::networkMain, this);
    }

    bool NetworkAdaptor::tryConnectToServer() {
        ENetPeer* serverPeer = nullptr;
        if (m_serverAddress.empty()) {
            moe::Logger::critical("Network peer address not set");
            return false;
        }

        ENetAddress address;
        if (enet_address_set_host(&address, m_serverAddress.data()) != 0) {
            moe::Logger::critical("Failed to resolve server address: {}", m_serverAddress);
            return false;
        }
        address.port = m_serverPort;

        ENetHost* client = enet_host_create(
                nullptr,
                1,
                MAX_CHANNELS,
                0,
                0);

        moe::Logger::info("Connecting to server at {}:{}", m_serverAddress, m_serverPort);
        serverPeer = enet_host_connect(client, &address, MAX_CHANNELS, 0);
        if (serverPeer == nullptr) {
            moe::Logger::critical("Failed to create ENet peer for server at {}:{}", m_serverAddress, m_serverPort);
            enet_host_destroy(client);
            return false;
        } else {
            moe::Logger::info("ENet peer created for server at {}:{}", m_serverAddress, m_serverPort);
        }

        ENetEvent event;
        int serviceResult = enet_host_service(client, &event, 5000);
        if (serviceResult <= 0 || event.type != ENET_EVENT_TYPE_CONNECT) {
            moe::Logger::error("Failed to connect to server at {}:{}", m_serverAddress, m_serverPort);
            enet_peer_reset(serverPeer);
            enet_host_destroy(client);
            return false;
        }

        moe::Logger::info("Successfully connected to server at {}:{}", m_serverAddress, m_serverPort);

        m_serverPeer = serverPeer;
        m_client = client;

        return true;
    }

    void NetworkAdaptor::handleEnetEvent(const ENetEvent& event) {
        switch (event.type) {
            case ENET_EVENT_TYPE_RECEIVE:
                moe::Logger::info(
                        "Received packet of length {} on channel {} from server",
                        event.packet->dataLength,
                        event.channelID);
                {
                    m_receiveQueue->enqueue(
                            {moe::Vector<uint8_t>(
                                    event.packet->data,
                                    event.packet->data + event.packet->dataLength)});
                }
                enet_packet_destroy(event.packet);
                break;
            case ENET_EVENT_TYPE_DISCONNECT:
                moe::Logger::info("Disconnected from server");
                m_running = false;
                break;
            default:
                break;
        }
    }

    void NetworkAdaptor::handleEnetSendRequest() {
        auto& sendQueue = *m_sendQueue;

        // ! fixme: currently the enet_packet_create copies the data,
        // ! for better performance add flag ENET_PACKET_FLAG_NO_ALLOCATE and manage memory manually
        // ! but just leave it for now
        TransmitSend data;
        size_t sendCount = 0;
        while (sendQueue.try_dequeue(data)) {
            auto flags = data.reliable
                                 ? ENET_PACKET_FLAG_RELIABLE
                                 : 0;
            ENetPacket* packet = enet_packet_create(
                    data.payload.data(),
                    data.payload.size(),
                    flags);

            // to avoid channel conflict
            if (data.reliable) {
                enet_peer_send(m_serverPeer, Channels::RELIABLE, packet);
            } else {
                enet_peer_send(m_serverPeer, Channels::UNRELIABLE, packet);
            }

            // prevent flooding the network in one cycle
            sendCount++;
            if (sendCount >= MAX_SEND_REQUEST_PER_CYCLE) {
                moe::Logger::warn(
                        "NetworkAdaptor::handleEnetSendRequest: reached max send request "
                        "per cycle limit ({})",
                        MAX_SEND_REQUEST_PER_CYCLE);
                break;
            }
        }

        // flush
        enet_host_flush(m_client);
    }

    void NetworkAdaptor::networkMain() {
        moe::Logger::info("Network thread started");
        moe::Logger::setThreadName("Network");

        if (enet_initialize() != 0) {
            moe::Logger::critical("An error occurred while initializing ENet");
            m_running = false;
            return;
        }

        if (!tryConnectToServer()) {
            moe::Logger::error("Failed to connect to server, exiting network thread");
            enet_deinitialize();
            m_running = false;
            return;
        }

        MOE_ASSERT(m_serverPeer != nullptr, "Server is null");
        MOE_ASSERT(m_client != nullptr, "Client is null");

        while (m_running) {
            ENetEvent event;
            while (enet_host_service(m_client, &event, NETWORK_LOOP_TIME_WAIT_MS) > 0) {
                handleEnetEvent(event);
            }

            // process send requests
            handleEnetSendRequest();
        }

        enet_deinitialize();
    }

    void NetworkAdaptor::sendData(moe::Span<const uint8_t> data, bool reliable) {
        auto sizeApprox = m_sendQueue->size_approx();
        if (sizeApprox > MAX_PENDING_SEND_REQUESTS) {
            moe::Logger::warn(
                    "NetworkAdaptor::sendData: "
                    "send queue size(~{}) reaching limit({}), dropping packet",
                    sizeApprox,
                    MAX_PENDING_SEND_REQUESTS);
            return;
        }

        if (data.empty()) {
            moe::Logger::warn("NetworkAdaptor::sendData: trying to send empty data");
            return;
        }

        bool sendResult = m_sendQueue->enqueue({moe::Vector<uint8_t>(data.begin(), data.end()), reliable});
        if (!sendResult) {
            moe::Logger::error("NetworkAdaptor::sendData: failed to enqueue send data");
        }
    }

    moe::Optional<TransmitRecv> NetworkAdaptor::tryReceiveData() {
        if (!m_running) {
            return std::nullopt;
        }

        TransmitRecv recvData;
        if (m_receiveQueue->try_dequeue(recvData)) {
            return recvData;
        }
        return std::nullopt;
    }
}// namespace game