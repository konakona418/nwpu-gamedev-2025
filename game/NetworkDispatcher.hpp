#pragma once

#include "NetworkAdaptor.hpp"

#include "Math/Common.hpp"

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
    X(::moe::net::ChatMessageEvent, ChatMessageEvent)                   \
    X(::moe::net::PurchaseEvent, PurchaseEvent)

namespace game {
    struct NetworkDispatcher : public moe::Meta::NonCopyable<NetworkDispatcher> {
    public:
        static constexpr uint32_t MAX_PLAYER_UPDATE_BUFFER_SIZE = 256;

        struct LastTimeSync {
            uint64_t lastServerTick{0};

            // ! fixme: this is suboptimal
            // ! a more robust time sync mechanism is needed
            // ! > sending time sync packets in unreliable channel periodically
            // ! > calculating clock offset between server and client
            // ! > using clock offset to adjust local time
            uint64_t roundTripTimeMs{0};
        };

        // ! fixme: consider concurrent queue for event queues
        struct Queues {
#define X(fbs_type, queue_name) ::moe::Deque<fbs_type##T> queue##queue_name;
            _GAME_NETWORK_DISPATCHER_QUEUES_XXX()
#undef X
        };

        struct PlayerUpdateData {
            glm::vec3 position;
            glm::vec3 velocity;
            glm::vec3 heading;
            uint64_t physicsTick;

            PlayerUpdateData(
                    const glm::vec3& pos,
                    const glm::vec3& vel,
                    const glm::vec3& head,
                    uint64_t tick)
                : position(pos),
                  velocity(vel),
                  heading(head),
                  physicsTick(tick) {}

            PlayerUpdateData() = default;
        };

        // a bare queue cause concurrency issues
        // we use concurrent queue instead
        using PlayerUpdateDataQueue = moodycamel::ConcurrentQueue<PlayerUpdateData>;

        explicit NetworkDispatcher(NetworkAdaptor* adaptor)
            : m_networkAdaptor(adaptor) {}

        void dispatchReceiveData();

        Queues& getQueues() { return *m_queues; }

        LastTimeSync& getLastTimeSync() { return m_lastTimeSync; }

        void registerPlayerUpdateBuffer(uint16_t playerTempId) {
            std::lock_guard lock(m_playerUpdateBufferMapMutex);// still, protect map access
            if (m_playerUpdateBufferMap.find(playerTempId) != m_playerUpdateBufferMap.end()) {
                moe::Logger::warn(
                        "NetworkDispatcher::registerPlayerUpdateBuffer: "
                        "player temp ID {} update buffer already registered",
                        playerTempId);
                return;
            }
            m_playerUpdateBufferMap[playerTempId] = PlayerUpdateDataQueue();
        }

        void unregisterPlayerUpdateBuffer(uint16_t playerTempId) {
            std::lock_guard lock(m_playerUpdateBufferMapMutex);
            m_playerUpdateBufferMap.erase(playerTempId);
        }

        void clearPlayerUpdateBuffer() {
            std::lock_guard lock(m_playerUpdateBufferMapMutex);
            m_playerUpdateBufferMap.clear();
        }

        moe::Optional<PlayerUpdateData> getPlayerUpdate(uint16_t tempId);

    private:
        NetworkAdaptor* m_networkAdaptor;
        moe::UniquePtr<Queues> m_queues = std::make_unique<Queues>();

        moe::UnorderedMap<uint16_t, PlayerUpdateDataQueue> m_playerUpdateBufferMap;
        std::mutex m_playerUpdateBufferMapMutex;

        LastTimeSync m_lastTimeSync;

        void dispatchReceivedEvent(const moe::net::ReceivedNetMessage* deserializedPacket);

        void handlePlayerUpdateEvent(const moe::net::ReceivedNetMessage* deserializedPacket);
    };
}// namespace game