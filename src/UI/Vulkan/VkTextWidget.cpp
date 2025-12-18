#include "UI/Vulkan/VkTextWidget.hpp"

MOE_BEGIN_NAMESPACE

// ! fixme: naive implementation, improve later
static constexpr float FONT_SIZE_MULTIPLIER = 1.2f;

LayoutSize VkTextWidget::preferredSize(const LayoutConstraints& constraints) const {
    if (m_fontId == NULL_FONT_ID || m_text.empty()) {
        return clampConstraintMinMax(LayoutSize{0, 0}, constraints);
    }

    auto metrics = m_font->measureText(m_text, m_fontSize);

    float finalWidth = metrics.width;

    float minHeightThreshold = m_fontSize * FONT_SIZE_MULTIPLIER;
    float finalHeight = Math::max(metrics.height, minHeightThreshold);

    finalWidth += (m_padding.left + m_padding.right);
    finalHeight += (m_padding.top + m_padding.bottom);

    return clampConstraintMinMax(
            LayoutSize{finalWidth, finalHeight},
            constraints);
}

void VkTextWidget::layout(const LayoutRect& rectAssigned) {
    m_calculatedBounds = rectAssigned;
    m_lines.clear();

    if (m_fontId == NULL_FONT_ID || m_text.empty()) return;

    float maxWidth = rectAssigned.width;
    float maxHeight = rectAssigned.height;

    float lineHeight = m_fontSize * FONT_SIZE_MULTIPLIER;

    float visibilityThreshold = m_fontSize * 0.5f;
    size_t currentIdx = 0;
    float currentY = 0.0f;
    size_t totalLen = m_text.length();

    while (currentIdx < totalLen) {
        if (currentY + visibilityThreshold > maxHeight) {
            break;
        };

        size_t low = 1;
        size_t high = totalLen - currentIdx;
        size_t bestCount = 0;

        while (low <= high) {
            size_t mid = low + (high - low) / 2;
            std::u32string_view testStr(m_text.data() + currentIdx, mid);
            auto metrics = m_font->measureText(testStr, m_fontSize);

            if (metrics.width <= maxWidth) {
                bestCount = mid;
                low = mid + 1;
            } else {
                high = mid - 1;
            }
        }

        // prevent infinite loop
        if (bestCount == 0) {
            if (maxWidth > 0) {
                bestCount = 1;
            } else {
                break;
            };
        }

        std::u32string lineText = m_text.substr(currentIdx, bestCount);

        m_lines.push_back(Line{lineText, currentY});

        currentIdx += bestCount;
        currentY += lineHeight;
    }
}

void VkTextWidget::render(VulkanRenderObjectBus& renderer) {
    if (m_fontId == NULL_FONT_ID) {
        Logger::warn("VkTextWidget render called with NULL_FONT_ID");
        return;
    }

    const auto& bounds = this->calculatedBounds();

    for (const auto& line: this->m_lines) {
        renderer.submitTextSpriteRender(
                m_fontId,
                this->m_fontSize,
                line.text,
                Transform{}.setPosition(glm::vec3{bounds.x, bounds.y + line.yOffset, 0.0f}),
                this->m_color);
    }
}

MOE_END_NAMESPACE