#include "State/PurchaseState.hpp"

#include "State/GamePlayState.hpp"

#include "State/GamePlayData.hpp"

#include "Localization.hpp"
#include "Registry.hpp"

#include "FlatBuffers/Generated/Sent/receiveGamingPacket_generated.h"

namespace game::State {

#define _GAME_PURCHASE_STATE_ITEM_NAME_TO_ENUM_MAP_XXX()                             \
    X("Glock", PurchaseState::Items::Glock, ::myu::net::Weapon::GLOCK)               \
    X("USP", PurchaseState::Items::USP, ::myu::net::Weapon::USP)                     \
    X("Desert Eagle", PurchaseState::Items::DesertEagle, ::myu::net::Weapon::DEAGLE) \
    X("AK47", PurchaseState::Items::AK47, ::myu::net::Weapon::AK47)                  \
    X("M4A1", PurchaseState::Items::M4A1, ::myu::net::Weapon::M4A1)

    const moe::Pair<moe::String, PurchaseState::Items> PurchaseState::s_itemNameToEnumMap[] = {
#define X(name, enumVal, _) {name, enumVal},
            _GAME_PURCHASE_STATE_ITEM_NAME_TO_ENUM_MAP_XXX()
#undef X
    };

    static I18N PURCHASE_MENU_TITLE("purchase_menu.title", U"Purchase Phase");
    static I18N PURCHASE_MENU_BALANCE("purchase_menu.balance", U"Balance: {}");

    moe::StringView purchaseStateItemToString(PurchaseState::Items item) {
        switch (item) {
#define X(name, enumVal, _) \
    case enumVal:           \
        return name;
            _GAME_PURCHASE_STATE_ITEM_NAME_TO_ENUM_MAP_XXX()
#undef X
            case PurchaseState::Items::None:
                break;
        }
        MOE_ASSERT(false, "Invalid PurchaseState::Items enum value");
    }

    PurchaseState::Items purchaseStateStringToItemEnum(moe::StringView itemName) {
        for (const auto& [name, enumVal]: PurchaseState::s_itemNameToEnumMap) {
            if (name == itemName) {
                return enumVal;
            }
        }
        return PurchaseState::Items::None;
    }

    void PurchaseState::onEnter(GameManager& ctx) {
        moe::Logger::info("Entering PurchaseState");

        auto fontId = m_fontLoader.generate().value_or(moe::NULL_FONT_ID);
        if (fontId == moe::NULL_FONT_ID) {
            moe::Logger::error("PurchaseState::onEnter: failed to load font");
            return;
        }
        m_fontId = fontId;

        // setup UI
        auto [width, height] = ctx.renderer().getCanvasSize();
        m_rootWidget = moe::makeRef<moe::RootWidget>((float) width, (float) height);

        m_containerWidget =
                moe::Ref(new moe::VkBoxWidget(moe::BoxLayoutDirection::Vertical));
        m_containerWidget->setBackgroundColor(moe::Color{0.f, 0.f, 0.f, 0.75f});
        m_containerWidget->setPadding({20.f, 20.f, 20.f, 20.f});

        m_containerWidget->setAlign(moe::BoxAlign::Center);
        m_containerWidget->setJustify(moe::BoxJustify::Center);

        m_titleTextWidget = moe::makeRef<moe::VkTextWidget>(
                PURCHASE_MENU_TITLE.get(),
                m_fontId,
                36.f,
                moe::Colors::White);
        m_titleTextWidget->setMargin({10.f, 0.f, 10.f, 20.f});
        m_containerWidget->addChild(m_titleTextWidget);

        for (const auto& [itemName, itemEnum]: s_itemNameToEnumMap) {
            auto itemButton = moe::makeRef<moe::VkButtonWidget>(
                    moe::VkButtonWidget::TextPref{
                            utf8::utf8to32(itemName),
                            m_fontId,
                            24.f,
                            moe::Color::fromNormalized(50, 50, 50, 255),
                    },
                    moe::VkButtonWidget::ButtonPref{
                            .preferredSize = {200.f, 50.f},
                    });
            itemButton->setMargin({10.f, 10.f, 10.f, 10.f});

            m_containerWidget->addChild(itemButton);
            m_itemButtonWidgets.push_back(itemButton);
        }

        auto gamePlaySharedData =
                Registry::getInstance().get<GamePlaySharedData>();
        m_lastKnownBalance = gamePlaySharedData->playerBalance;

        m_balanceTextWidget = moe::makeRef<moe::VkTextWidget>(
                Util::formatU32(PURCHASE_MENU_BALANCE.get(), gamePlaySharedData->playerBalance),
                m_fontId,
                24.f,
                moe::Colors::Yellow);
        m_balanceTextWidget->setMargin({10.f, 10.f, 10.f, 10.f});
        m_containerWidget->addChild(m_balanceTextWidget);

        m_rootWidget->addChild(m_containerWidget);

        m_rootWidget->layout();

        auto& input = ctx.input();
        input.addProxy(&m_inputProxy);
        m_inputProxy.setMouseState(true);// enable mouse cursor
    }

    void PurchaseState::onExit(GameManager& ctx) {
        moe::Logger::info("Exiting PurchaseState");

        auto& input = ctx.input();
        m_inputProxy.setMouseState(false);// disable mouse cursor
        input.removeProxy(&m_inputProxy);

        m_rootWidget.reset();
        m_itemButtonWidgets.clear();
    }

    myu::net::Weapon purchaseStateItemToWeaponEnum(PurchaseState::Items item) {
        switch (item) {
#define X(_, enumVal, weaponEnum) \
    case enumVal:                 \
        return weaponEnum;
            _GAME_PURCHASE_STATE_ITEM_NAME_TO_ENUM_MAP_XXX()
#undef X
            case PurchaseState::Items::None:
                break;
        }
        MOE_ASSERT(false, "Invalid PurchaseState::Items enum value");
    }

    void constructSendPurchaseReq(GameManager& ctx, PurchaseState::Items item) {
        auto* gamePlaySharedData =
                Registry::getInstance().get<GamePlaySharedData>();
        if (!gamePlaySharedData) {
            moe::Logger::error("PurchaseState::constructPurchaseRequest: GamePlaySharedData not found");
            return;
        }

        auto playerTempId = gamePlaySharedData->playerTempId;


        flatbuffers::FlatBufferBuilder fbb;
        auto purchaseEvent = myu::net::CreatePurchaseEvent(
                fbb, playerTempId,
                purchaseStateItemToWeaponEnum(item));

        auto time = game::Util::getTimePack();

        auto header = myu::net::CreatePacketHeader(
                fbb,
                time.physicsTick,
                time.currentTimeMillis);

        auto message = myu::net::CreateNetMessage(
                fbb,
                header,
                myu::net::PacketUnion::PurchaseEvent,
                purchaseEvent.Union());

        fbb.Finish(message);

        // send to server
        ctx.network().sendData(fbb.GetBufferSpan(), true);
    }

    void PurchaseState::onUpdate(GameManager& ctx, float deltaTime) {
        auto gamePlaySharedData =
                Registry::getInstance().get<GamePlaySharedData>();
        if (gamePlaySharedData->playerBalance != m_lastKnownBalance) {
            m_lastKnownBalance = gamePlaySharedData->playerBalance;
            m_balanceTextWidget->setText(
                    Util::formatU32(
                            PURCHASE_MENU_BALANCE.get(),
                            gamePlaySharedData->playerBalance));
            m_rootWidget->layout();
        }

        auto& renderer = ctx.renderer().getBus<moe::VulkanRenderObjectBus>();
        m_containerWidget->render(renderer);
        m_titleTextWidget->render(renderer);

        for (auto& itemButton: m_itemButtonWidgets) {
            itemButton->render(renderer);
            auto cursorPos = m_inputProxy.getMousePosition();
            auto lmbPressed = m_inputProxy.getMouseButtonState().pressedLMB;
            if (itemButton->checkButtonState(cursorPos, lmbPressed)) {
                auto text = utf8::utf32to8(itemButton->text());
                moe::Logger::info("Item button '{}' clicked", text);
                auto item = purchaseStateStringToItemEnum(text);

                moe::Logger::info("Constructing and sending purchase request for item '{}'",
                                  purchaseStateItemToString(item));
                constructSendPurchaseReq(ctx, item);

                // we dont handle response here; main gameplay state will handle purchase result
            }
        }

        m_balanceTextWidget->render(renderer);
    }
}// namespace game::State