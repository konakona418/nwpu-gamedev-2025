#pragma once

#include "UI/Widget.hpp"

MOE_BEGIN_NAMESPACE

struct RootWidget : public Widget {
public:
    RootWidget() = default;
    explicit RootWidget(const LayoutRect& viewportRect)
        : m_viewportRect(viewportRect) {}
    explicit RootWidget(float width, float height)
        : m_viewportRect{0.f, 0.f, width, height} {}

    WidgetType type() const override {
        return WidgetType::Container;
    }

    LayoutSize preferredSize(const LayoutConstraints& constraints) const override {
        // this have no actual effect
        return LayoutSize{
                m_viewportRect.width,
                m_viewportRect.height,
        };
    }

    void layout(const LayoutRect& rectAssigned) override {
        m_calculatedBounds = rectAssigned;
        for (auto child: m_children) {
            child->layout(rectAssigned);
        }
    }

    void layout() {
        this->layout(m_viewportRect);
    }

    void setViewportRect(const LayoutRect& viewportRect) {
        m_viewportRect = viewportRect;
    }

    const LayoutRect& viewportRect() const {
        return m_viewportRect;
    }

protected:
    LayoutRect m_viewportRect;
};

MOE_END_NAMESPACE