#include "NetworkDispatcher.hpp"

namespace game {
    void NetworkDispatcher::dispatchReceivedEvent(const moe::net::ReceivedNetMessage* deserializedPacket) {
        auto* eventPayload =
                static_cast<const moe::net::GameEvent*>(deserializedPacket->packet());

        auto type = eventPayload->payload_type();
        switch (type) {
#define X(fbs_type, queue_name)                                          \
    case moe::net::EventData::queue_name: {                              \
        auto* eventData = eventPayload->payload_as_##queue_name();       \
        if (eventData) {                                                 \
            m_queues->queue##queue_name.push_back(*eventData->UnPack()); \
        } else {                                                         \
            moe::Logger::warn(                                           \
                    "NetworkDispatcher::dispatchReceiveData: "           \
                    "failed to deserialize event of type " #fbs_type);   \
        }                                                                \
        break;                                                           \
    }

            _GAME_NETWORK_DISPATCHER_QUEUES_XXX()
#undef X

            default: {
                moe::Logger::warn(
                        "NetworkDispatcher::dispatchReceiveData: "
                        "unknown event type received");
                break;
            }
        }
    }

    void NetworkDispatcher::handlePlayerUpdateEvent(const moe::net::ReceivedNetMessage* deserializedPacket) {
        auto* eventData = deserializedPacket->packet_as_AllPlayerUpdate();
        if (!eventData) {
            moe::Logger::warn(
                    "NetworkDispatcher::handlePlayerUpdateEvent: "
                    "failed to deserialize AllPlayerUpdate event");
            return;
        }

        for (const auto playerUpdate: *eventData->updates()) {
            uint16_t tempId = playerUpdate->tempId();
            auto it = m_playerUpdateBufferMap.find(tempId);
            if (it == m_playerUpdateBufferMap.end()) {
                moe::Logger::warn(
                        "NetworkDispatcher::handlePlayerUpdateEvent: "
                        "no update buffer found for player temp ID {}",
                        tempId);
                continue;
            }

            auto& updateBuffer = it->second;
            if (updateBuffer.size() >= MAX_PLAYER_UPDATE_BUFFER_SIZE) {
                // drop oldest update
                moe::Logger::warn(
                        "NetworkDispatcher::handlePlayerUpdateEvent: "
                        "player temp ID {} update buffer full (size: {}), dropping oldest update. "
                        "Are updates not being consumed correctly?",
                        tempId,
                        updateBuffer.size());
                updateBuffer.pop_front();
            }

            auto pos = playerUpdate->pos();
            auto vel = playerUpdate->vel();
            auto head = playerUpdate->head();

            updateBuffer.emplace_back(
                    glm::vec3(pos->x(), pos->y(), pos->z()),
                    glm::vec3(vel->x(), vel->y(), vel->z()),
                    glm::vec3(head->x(), head->y(), head->z()),
                    deserializedPacket->header()->serverTick());
        }
    }

    void NetworkDispatcher::dispatchReceiveData() {
        auto& network = *m_networkAdaptor;
        while (auto packet_ = network.tryReceiveData()) {
            auto& packet = *packet_;

            auto* deserializedPacket = moe::net::GetReceivedNetMessage(packet.payload.data());
            if (!deserializedPacket) {
                moe::Logger::warn("NetworkDispatcher::dispatchReceiveData: failed to deserialize packet");
                continue;
            }

            // update last time sync info
            {
                auto* header = deserializedPacket->header();

                m_lastTimeSync.lastServerTick = header->serverTick();
                //m_lastTimeSync.lastServerTimeMillis = header->serverTsMs();
                // !!!
                // we no longer use server time millis for synchronization
                // instead, we use round trip time to estimate latency
                // this is because the system clock on server and client are **NOT** synchronized
                m_lastTimeSync.roundTripTimeMs = packet.roundTripTimeMs;
            }

            auto payloadType = deserializedPacket->packet_type();

            switch (payloadType) {
                case moe::net::ReceivedPacketUnion::GameEvent: {
                    dispatchReceivedEvent(deserializedPacket);
                    break;
                }
                case moe::net::ReceivedPacketUnion::AllPlayerUpdate: {
                    handlePlayerUpdateEvent(deserializedPacket);
                    break;
                }
                default: {
                    moe::Logger::warn(
                            "NetworkDispatcher::dispatchReceiveData: "
                            "unknown packet type received");
                    break;
                }
            }
        }
    }

    moe::Optional<NetworkDispatcher::PlayerUpdateData> NetworkDispatcher::getPlayerUpdate(uint16_t tempId) {
        auto it = m_playerUpdateBufferMap.find(tempId);
        if (it == m_playerUpdateBufferMap.end()) {
            moe::Logger::warn(
                    "NetworkDispatcher::getPlayerUpdate: "
                    "no update buffer found for player temp ID {}",
                    tempId);
            return std::nullopt;
        }

        auto& updateBuffer = it->second;
        if (updateBuffer.empty()) {
            return std::nullopt;
        }

        auto update = updateBuffer.front();
        updateBuffer.pop_front();
        return update;
    }
}// namespace game