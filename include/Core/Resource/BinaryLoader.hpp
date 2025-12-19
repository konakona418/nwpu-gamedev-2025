#pragma once

#include "Core/Common.hpp"
#include "Core/FileReader.hpp"
#include "Core/Ref.hpp"
#include "Core/Resource/BinaryBuffer.hpp"


MOE_BEGIN_NAMESPACE

struct BinaryFilePath {
    StringView path;

    BinaryFilePath(StringView p)
        : path(p) {}
};

struct BinaryLoader {
public:
    using value_type = Ref<BinaryBuffer>;

    explicit BinaryLoader(BinaryFilePath filePath)
        : m_filePath(filePath.path) {}

    Optional<value_type> generate() {
        size_t fileSize = 0;
        auto data = FileReader::s_instance->readFile(m_filePath, fileSize);
        if (!data) {
            return {};
        }

        return Ref(new BinaryBuffer(std::move(*data)));
    }

    uint64_t hashCode() const {
        return std::hash<String>()(m_filePath);
    }

    String paramString() const {
        return fmt::format(
                "binary_loader_{}_{}",
                m_filePath,
                hashCode());
    }

private:
    String m_filePath;
};

MOE_END_NAMESPACE