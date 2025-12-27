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
                    // todo: handle player update
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
    }// namespace game
}// namespace game