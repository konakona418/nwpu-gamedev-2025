#include "State/CreditsState.hpp"

#include "GameManager.hpp"
#include "Localization.hpp"

namespace game::State {
    static I18N CREDITS_TITLE("credits.title", U"Credits");

    static I18N CREDITS_TITLE_PROGRAMMING("credits.role.programming", U"Programming");
    static I18N CREDITS_ROLE_ENGINE_ARCHITECTURE("credits.role.engine_architecture", U"Engine Architecture");
    static I18N CREDITS_ROLE_GAME_DESIGN("credits.role.game_design", U"Game Design");
    static I18N CREDITS_ROLE_NETWORKING("credits.role.networking", U"Server Architecture & Networking");
    static I18N CREDITS_ROLE_TECHNICAL_ART("credits.role.technical_art", U"Technical Art");

    static I18N CREDITS_TITLE_ART_DESIGN("credits.role.art_design", U"Art & Design");
    static I18N CREDITS_ROLE_MODELING("credits.role.modeling", U"Modeling");
    static I18N CREDITS_ROLE_UI_UX("credits.role.ui_ux", U"UI/UX Design");

    static I18N CREDITS_TITLE_MUSIC_SOUND("credits.role.music_sound", U"Music & Sound");
    static I18N CREDITS_ROLE_MUSIC_COMPOSITION("credits.role.music_composition", U"Music");
    static I18N CREDITS_ROLE_SOUND_EFFECTS("credits.role.sound_effects", U"Sound Effects");

    static I18N CREDITS_TITLE_DOCUMENTATION("credits.role.documentation", U"Documentation");
    static I18N CREDITS_ROLE_DOCUMENTATION("credits.role.documentation", U"Documentation And Report Writing");

    static I18N CREDITS_TITLE_TESTING("credits.role.testing", U"Testing");

    static I18N CREDITS_TITLE_THANKS("credits.role.thanks", U"Special Thanks");

    struct CreditEntry {
        moe::U32String name;
        moe::U32String role;
    };

    struct CreditGroup {
        moe::U32String title;
        moe::Vector<CreditEntry> entries;
    };

    static const moe::Vector<CreditGroup> s_creditGroups = {
            {
                    CREDITS_TITLE_PROGRAMMING.get(),
                    {
                            {U"李梓萌，冯于洋", CREDITS_ROLE_ENGINE_ARCHITECTURE.get()},
                            {U"冯于洋", CREDITS_ROLE_NETWORKING.get()},
                            {U"李佳睿", CREDITS_ROLE_GAME_DESIGN.get()},
                            {U"吴焕石，李梓萌", CREDITS_ROLE_TECHNICAL_ART.get()},
                    },
            },
            {
                    CREDITS_TITLE_ART_DESIGN.get(),
                    {
                            {U"吴焕石", CREDITS_ROLE_MODELING.get()},
                            {U"吴焕石", CREDITS_ROLE_UI_UX.get()},
                    },
            },
            {
                    CREDITS_TITLE_MUSIC_SOUND.get(),
                    {
                            {U"吴焕石", CREDITS_ROLE_MUSIC_COMPOSITION.get()},
                            {U"滕艺斐", CREDITS_ROLE_SOUND_EFFECTS.get()},
                    },
            },
            {
                    CREDITS_TITLE_DOCUMENTATION.get(),
                    {
                            {U"滕艺斐", CREDITS_ROLE_DOCUMENTATION.get()},
                    },
            },
    };

    void CreditsState::onEnter(GameManager& ctx) {
        moe::Logger::info("Entering CreditsState");

        auto& input = ctx.input();
        input.addKeyMapping("credit_exit", GLFW_KEY_ESCAPE);

        input.addProxy(&m_inputProxy);

        m_inputProxy.setMouseState(true);

        auto fontId = m_fontLoader.generate().value_or(moe::NULL_FONT_ID);
        if (fontId == moe::NULL_FONT_ID) {
            moe::Logger::error("CreditsState::onEnter: failed to load font");
            return;
        }
        m_fontId = fontId;

        // todo
        auto canvas = ctx.renderer().getCanvasSize();
        m_rootWidget = moe::makeRef<moe::RootWidget>(canvas.first, canvas.second);

        m_containerWidget =
                moe::makeRef<moe::VkBoxWidget>(moe::BoxLayoutDirection::Vertical);
        m_containerWidget->setPadding({20.f, 20.f, 20.f, 20.f});
        m_containerWidget->setBackgroundColor(moe::Color{0.f, 0.f, 0.f, 0.75f});
        m_containerWidget->setAlign(moe::BoxAlign::Center);
        m_containerWidget->setJustify(moe::BoxJustify::Center);

        for (const auto& group: s_creditGroups) {
            auto titleWidget = moe::makeRef<moe::VkTextWidget>(
                    group.title,
                    m_fontId,
                    24.f,
                    moe::Colors::Yellow);
            titleWidget->setMargin({10.f, 0.f, 10.f, 10.f});
            m_containerWidget->addChild(titleWidget);

            m_creditsTextWidgets.push_back(titleWidget);

            for (const auto& entry: group.entries) {
                auto entryWidget = moe::makeRef<moe::VkTextWidget>(
                        Util::formatU32(
                                U"{} - {}",
                                utf8::utf32to8(entry.name),
                                utf8::utf32to8(entry.role)),
                        m_fontId,
                        20.f,
                        moe::Colors::White);
                entryWidget->setMargin({5.f, 0.f, 5.f, 5.f});
                m_containerWidget->addChild(entryWidget);

                m_creditsTextWidgets.push_back(entryWidget);
            }
        }
        m_rootWidget->addChild(m_containerWidget);

        m_rootWidget->layout();
    }

    void CreditsState::onExit(GameManager& ctx) {
        moe::Logger::info("Exiting CreditsState");

        auto& input = ctx.input();
        input.removeKeyMapping("credit_exit");
        m_inputProxy.setMouseState(false);
        input.removeProxy(&m_inputProxy);
    }

    void CreditsState::onUpdate(GameManager& ctx, float deltaTime) {
        if (m_inputProxy.isKeyPressed("credit_exit") || m_inputProxy.getMouseButtonState().pressedLMB) {
            moe::Logger::info("Exiting CreditsState on user request");

            ctx.queueFree(this->intoRef());
            return;
        }

        auto& renderctx = ctx.renderer().getBus<moe::VulkanRenderObjectBus>();

        m_containerWidget->render(renderctx);
        for (auto& textWidget: m_creditsTextWidgets) {
            textWidget->render(renderctx);
        }
    }
}// namespace game::State