#pragma once

#include "Core/Common.hpp"

#include "Math/Color.hpp"

#include "UI/Common.hpp"
#include "UI/Widget.hpp"

MOE_BEGIN_NAMESPACE

struct TextWidget : public Widget {
public:
    struct Line {
        U32String text;
        float yOffset;
    };

    explicit TextWidget(
            U32StringView text = U"",
            float fontSize = 16.f,
            Color color = Color{255, 255, 255, 255})
        : m_text(text), m_fontSize(fontSize), m_color(color) {}

    ~TextWidget() override = default;

    WidgetType type() const override {
        return WidgetType::Unknown;
    }

    void addChild(Ref<Widget> /*child*/) override {
        MOE_ASSERT(false, "TextWidget cannot have children");
    }

    LayoutSize preferredSize(const LayoutConstraints& constraints) const override;

    void layout(const LayoutRect& rectAssigned) override;

    void setText(U32StringView text) {
        m_text = text;
    }

    U32StringView text() const {
        return m_text;
    }

    void setFontSize(float fontSize) {
        m_fontSize = fontSize;
    }

    float fontSize() const {
        return m_fontSize;
    }

    void setColor(const Color& color) {
        m_color = color;
    }

    Color color() const {
        return m_color;
    }

    Span<const Line> lines() const {
        return Span<const Line>(m_lines.data(), m_lines.size());
    }

protected:
    U32String m_text;

    float m_fontSize;
    Color m_color{255, 255, 255, 255};

    Vector<Line> m_lines;
};

MOE_END_NAMESPACE