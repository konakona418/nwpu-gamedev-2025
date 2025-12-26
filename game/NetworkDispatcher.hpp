#pragma once

#include "NetworkAdaptor.hpp"

#include "FlatBuffers/Generated/Received/Main_generated.h"

#define _GAME_NETWORK_DISPATCHER_QUEUES_XXX()                           \
    X(::moe::net::PlayerJoinedEvent, PlayerJoinedEvent)                 \
    X(::moe::net::PlayerLeftEvent, PlayerLeftEvent)                     \
    X(::moe::net::GameStartedEvent, GameStartedEvent)                   \
    X(::moe::net::RoundPurchaseStartedEvent, RoundPurchaseStartedEvent) \
    X(::moe::net::RoundStartedEvent, RoundStartedEvent)                 \
    X(::moe::net::RoundEndedEvent, RoundEndedEvent)                     \
    X(::moe::net::GameEndedEvent, GameEndedEvent)                       \
    X(::moe::net::PlayerKilledEvent, PlayerKilledEvent)                 \
    X(::moe::net::BombPlantedEvent, BombPlantedEvent)                   \
    X(::moe::net::BombDefusedEvent, BombDefusedEvent)                   \
    X(::moe::net::ChatMessageEvent, ChatMessageEvent)

namespace game {
    struct NetworkDispatcher : public moe::Meta::NonCopyable<NetworkDispatcher> {
    public:
        struct LastTimeSync {
            uint64_t lastServerTick{0};

            // ! fixme: this is suboptimal
            // ! a more robust time sync mechanism is needed
            // ! > sending time sync packets in unreliable channel periodically
            // ! > calculating clock offset between server and client
            // ! > using clock offset to adjust local time
            uint64_t roundTripTimeMs{0};
        };

        struct Queues {
#define X(fbs_type, queue_name) ::moe::Deque<fbs_type##T> queue##queue_name;
            _GAME_NETWORK_DISPATCHER_QUEUES_XXX()
#undef X
        };

        explicit NetworkDispatcher(NetworkAdaptor* adaptor)
            : m_networkAdaptor(adaptor) {}

        void dispatchReceiveData();

        Queues& getQueues() { return *m_queues; }

        LastTimeSync& getLastTimeSync() { return m_lastTimeSync; }

    private:
        NetworkAdaptor* m_networkAdaptor;
        moe::UniquePtr<Queues> m_queues = std::make_unique<Queues>();

        LastTimeSync m_lastTimeSync;

        void dispatchReceivedEvent(const moe::net::ReceivedNetMessage* deserializedPacket);
    };
}// namespace game