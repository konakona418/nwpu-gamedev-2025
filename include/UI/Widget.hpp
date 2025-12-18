#pragma once

#include "UI/Common.hpp"

#include "Core/Common.hpp"
#include "Core/RefCounted.hpp"

MOE_BEGIN_NAMESPACE

struct Widget : public AtomicRefCounted<Widget> {
public:
    Widget() = default;
    virtual ~Widget() = default;

    virtual WidgetType type() const {
        return m_type;
    }

    virtual LayoutSize preferredSize(const LayoutConstraints& constraints) const = 0;

    virtual void layout(const LayoutRect& rectAssigned) = 0;

    virtual void addChild(Ref<Widget> child) {
        m_children.push_back(child);
        child->m_parent = this;
    }

    const Vector<Ref<Widget>>& children() const {
        return m_children;
    }

    void setMargin(const LayoutBorders& margin) {
        m_margin = margin;
    }

    const LayoutBorders& margin() const {
        return m_margin;
    }

    void setPadding(const LayoutBorders& padding) {
        m_padding = padding;
    }

    const LayoutBorders& padding() const {
        return m_padding;
    }

    const LayoutRect& calculatedBounds() const {
        return m_calculatedBounds;
    }

protected:
    Widget* m_parent{nullptr};
    Vector<Ref<Widget>> m_children;

    LayoutBorders m_margin;
    LayoutBorders m_padding;

    LayoutRect m_calculatedBounds;

    WidgetType m_type{WidgetType::Unknown};
};

MOE_END_NAMESPACE