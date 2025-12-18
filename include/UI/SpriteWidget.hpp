#pragma once

#include "UI/Common.hpp"
#include "UI/Widget.hpp"

MOE_BEGIN_NAMESPACE

struct SpriteWidget : public Widget {
public:
    explicit SpriteWidget(const LayoutSize& spriteSize = LayoutSize{64.f, 64.f})
        : m_spriteSize(spriteSize) {
        m_type = WidgetType::AnySprite;
    }

    ~SpriteWidget() override = default;

    WidgetType type() const override {
        return WidgetType::AnySprite;
    }

    LayoutSize preferredSize(const LayoutConstraints& constraints) const override;

    void layout(const LayoutRect& rectAssigned) override;

protected:
    LayoutSize m_spriteSize;
    LayoutRect m_spriteRenderRect;
};

MOE_END_NAMESPACE
