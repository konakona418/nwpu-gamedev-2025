#pragma once

#include "Core/Common.hpp"
#include "Core/Meta/Feature.hpp"
#include "Core/RefCounted.hpp"

namespace game {
    constexpr uint16_t INVALID_PLAYER_TEMP_ID = 0xFFFF;

    struct NetworkDispatcher;

    enum class GamePlayerTeam {
        None,
        T,
        CT,
    };
    struct GamePlayer {
        uint16_t tempId{INVALID_PLAYER_TEMP_ID};
        moe::U32String name;
        GamePlayerTeam team{GamePlayerTeam::None};
    };

    struct GamePlaySharedData
        : public moe::AtomicRefCounted<GamePlaySharedData>,
          public moe::Meta::NonCopyable<GamePlaySharedData> {
        NetworkDispatcher* networkDispatcher{nullptr};

        uint16_t playerTempId{INVALID_PLAYER_TEMP_ID};
        uint32_t playerBalance{0};

        moe::UnorderedMap<uint16_t, GamePlayer> playersByTempId;

        uint32_t playerFireSequence{0};
        uint32_t playerMoveSequence{0};
    };
}// namespace game