#pragma once

#include "GameManager.hpp"
#include "GameState.hpp"

#include "AnyCache.hpp"
#include "FontLoader.hpp"
#include "Util.hpp"

#include "UI/RootWidget.hpp"
#include "UI/Vulkan/VkBoxWidget.hpp"
#include "UI/Vulkan/VkTextWidget.hpp"

#include "Core/Resource/BinaryLoader.hpp"
#include "Core/Resource/Launch.hpp"
#include "Core/Resource/Preload.hpp"
#include "Core/Resource/Secure.hpp"

namespace game::State {
    struct ChatboxState : public GameState {
    public:
        struct ChatItem {
            moe::U32String senderName;
            moe::U32String message;
            moe::Color color{moe::Colors::White};
            float timestamp{0.0f};
        };

        const moe::StringView getName() const override {
            return "ChatboxState";
        }

        void onEnter(GameManager& ctx) override;
        void onExit(GameManager& ctx) override;
        void onUpdate(GameManager& ctx, float deltaTime) override;

        void addChatMessage(moe::U32StringView senderName, moe::U32StringView message, const moe::Color& color) {
            m_pendingChatItems.push_back(
                    ChatItem{
                            .senderName = senderName.data(),
                            .message = message.data(),
                            .color = color,
                    });
        }

    private:
        moe::Deque<ChatItem> m_chatItems;
        float displayDurationSecs{10.0f};
        size_t maxChatItems{10};

        moe::Secure<game::AnyCacheLoader<game::FontLoader<
                moe::Preload<moe::Launch<game::AnyCacheLoader<moe::BinaryLoader>>>>>>
                m_fontId{
                        FontLoaderParam{48.0f, Util::glyphRangeChinese()},
                        moe::BinaryFilePath(moe::asset("assets/fonts/NotoSansSC-Regular.ttf"))};

        moe::Ref<moe::RootWidget> m_rootWidget{nullptr};
        moe::Ref<moe::VkBoxWidget> m_chatBoxWidget{nullptr};
        moe::Deque<moe::Ref<moe::VkTextWidget>> m_chatTextWidgets;

        moe::Vector<ChatItem> m_pendingChatItems;

        void handleChatItemsUpdate(GameManager& ctx, float deltaTime);
    };
}// namespace game::State