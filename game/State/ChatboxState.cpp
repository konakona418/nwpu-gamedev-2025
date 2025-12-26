#include "ChatboxState.hpp"

#include "Param.hpp"

#include "imgui.h"

namespace game::State {
    static ParamF CHATBOX_DISPLAY_DURATION_SECS("chatbox.display_duration_secs", 10.0f, ParamScope::UserConfig);
    static ParamI CHATBOX_MAX_ITEMS("chatbox.max_items", 10, ParamScope::UserConfig);

    void ChatboxState::onEnter(GameManager& ctx) {
        displayDurationSecs = CHATBOX_DISPLAY_DURATION_SECS.get();
        maxChatItems = static_cast<size_t>(CHATBOX_MAX_ITEMS.get());

        auto canvasSize = ctx.renderer().getCanvasSize();
        m_rootWidget = moe::Ref(new moe::RootWidget(canvasSize.first, canvasSize.second));

        auto chatBoxContainer = moe::makeRef<moe::VkBoxWidget>(moe::BoxLayoutDirection::Horizontal);

        chatBoxContainer->setJustify(moe::BoxJustify::Start);
        chatBoxContainer->setAlign(moe::BoxAlign::Center);

        m_chatBoxWidget = moe::Ref(new moe::VkBoxWidget(moe::BoxLayoutDirection::Vertical));
        m_chatBoxWidget->setPadding({5.f, 5.f, 5.f, 5.f});

        m_chatBoxWidget->setBackgroundColor(moe::Color::fromNormalized(0, 0, 0, 128));

        chatBoxContainer->addChild(m_chatBoxWidget);
        m_rootWidget->addChild(chatBoxContainer);

        m_rootWidget->layout();

        ctx.addDebugDrawFunction(
                "ChatboxState UI",
                [this, &ctx]() {
                    auto& renderer = ctx.renderer();
                    renderer.addImGuiDrawCommand([this]() {
                        {
                            static char senderName[64] = "Tester";
                            static char message[256] = "Hello, this is a test message!";
                            static float color[4] = {1.0f, 1.0f, 1.0f, 1.0f};

                            ImGui::Begin("ChatboxState Test Input");

                            ImGui::TextUnformatted("Send Test Message:");
                            ImGui::InputText("Sender Name", senderName, sizeof(senderName));
                            ImGui::InputText("Message", message, sizeof(message));
                            ImGui::ColorEdit4("Color", color);
                            if (ImGui::Button("Send Message")) {
                                addChatMessage(
                                        utf8::utf8to32(moe::StringView(senderName)),
                                        utf8::utf8to32(moe::StringView(message)),
                                        moe::Color(color[0], color[1], color[2], color[3]));
                            }
                            ImGui::End();
                        }
                        {
                            ImGui::Begin("ChatboxState Inspector");
                            ImGui::TextUnformatted("Chatbox Messages:");
                            if (m_chatItems.empty()) {
                                ImGui::TextUnformatted("No messages.");
                            } else {
                                if (ImGui::BeginListBox("##Chatbox_Messages", ImVec2(-FLT_MIN, 300.f))) {
                                    for (const auto& chatItem: m_chatItems) {
                                        ImGui::Text(
                                                "%s: %s (%.1f s)",
                                                utf8::utf32to8(chatItem.senderName).c_str(),
                                                utf8::utf32to8(chatItem.message).c_str(),
                                                chatItem.timestamp);
                                        ImGui::SameLine();

                                        ImGui::PushID(&chatItem);
                                        ImGui::ColorButton(
                                                "Color",
                                                ImVec4(
                                                        chatItem.color.r,
                                                        chatItem.color.g,
                                                        chatItem.color.b,
                                                        chatItem.color.a),
                                                ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop);
                                        ImGui::PopID();
                                    }

                                    ImGui::EndListBox();
                                }
                            }
                            ImGui::End();
                        }
                    });
                });
    }

    void ChatboxState::onExit(GameManager& ctx) {
        m_chatItems.clear();
        m_chatTextWidgets.clear();

        m_rootWidget.reset();
        m_chatBoxWidget.reset();
        for (auto chatTextWidget: m_chatTextWidgets) {
            chatTextWidget.reset();
        }

        ctx.removeDebugDrawFunction("ChatboxState UI");
    }

    void ChatboxState::handleChatItemsUpdate(GameManager& ctx, float deltaTime) {
        bool updated = false;

        // update chat item ts
        for (auto it = m_chatItems.begin(); it != m_chatItems.end();) {
            it->timestamp += deltaTime;
            if (it->timestamp >= displayDurationSecs) {
                it = m_chatItems.erase(it);
                if (!m_chatTextWidgets.empty()) {
                    m_chatBoxWidget->removeChild(m_chatTextWidgets.front());

                    m_chatTextWidgets.front().reset();
                    m_chatTextWidgets.pop_front();

                    updated = true;
                }
            } else {
                ++it;
            }
        }

        // enforce max chat items
        while (m_chatItems.size() > maxChatItems) {
            m_chatItems.pop_front();
            if (!m_chatTextWidgets.empty()) {
                m_chatBoxWidget->removeChild(m_chatTextWidgets.front());

                m_chatTextWidgets.front().reset();
                m_chatTextWidgets.pop_front();

                updated = true;
            }
        }

        // add pending chat items
        moe::Vector<char32_t> fullMessage;
        for (auto& pendingChatItem: m_pendingChatItems) {
            m_chatItems.push_back(pendingChatItem);

            fullMessage.reserve(
                    pendingChatItem.senderName.size() +
                    2 +// for ": "
                    pendingChatItem.message.size());
            fullMessage.assign(pendingChatItem.senderName.begin(), pendingChatItem.senderName.end());
            fullMessage.assign({U':', U' '});
            fullMessage.assign(pendingChatItem.message.begin(), pendingChatItem.message.end());

            auto chatTextWidget = moe::Ref(new moe::VkTextWidget(
                    fullMessage.data(),
                    m_fontId.generate().value_or(moe::NULL_FONT_ID),
                    16.f,
                    pendingChatItem.color));

            fullMessage.clear();
            chatTextWidget->setText(pendingChatItem.senderName + U": " + pendingChatItem.message);
            chatTextWidget->setColor(pendingChatItem.color);

            m_chatBoxWidget->addChild(chatTextWidget);
            m_chatTextWidgets.push_back(chatTextWidget);

            updated = true;
        }
        m_pendingChatItems.clear();

        if (updated) {
            m_rootWidget->layout();
        }
    }

    void ChatboxState::onUpdate(GameManager& ctx, float deltaTime) {
        handleChatItemsUpdate(ctx, deltaTime);

        // only render if there are chat items
        if (m_chatItems.empty()) {
            return;
        }

        auto& renderer = ctx.renderer().getBus<moe::VulkanRenderObjectBus>();
        m_chatBoxWidget->render(renderer);
        for (auto& chatTextWidget: m_chatTextWidgets) {
            chatTextWidget->render(renderer);
        }
    }
}// namespace game::State