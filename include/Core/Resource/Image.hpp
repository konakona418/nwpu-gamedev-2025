#pragma once

#include "Core/Common.hpp"
#include "Core/RefCounted.hpp"

MOE_BEGIN_NAMESPACE

struct Image : public AtomicRefCounted<Image> {
public:
    Image(Vector<uint8_t>&& data, int width, int height, int channels)
        : m_data(std::move(data)), m_width(width), m_height(height), m_channels(channels) {}

    const uint8_t* data() const { return m_data.data(); }

    int width() const { return m_width; }

    int height() const { return m_height; }

    int channels() const { return m_channels; }

private:
    Vector<uint8_t> m_data;
    int m_width{0};
    int m_height{0};
    int m_channels{0};
};

MOE_END_NAMESPACE