#pragma once

#include "Core/Common.hpp"
#include "Core/FileReader.hpp"
#include "Core/Meta/Generator.hpp"

#include <filesystem>
#include <fstream>

#include <fmt/core.h>

MOE_BEGIN_NAMESPACE

template<
        typename InnerGenerator,
        typename = Meta::EnableIfT<Meta::IsGeneratorV<InnerGenerator>>,
        typename = Meta::EnableIfT<Meta::IsBinarySerializableV<typename InnerGenerator::value_type>>>
struct Cached {
public:
    const String DEFAULT_CACHE_DIR = "./cache/";

    using value_type = typename InnerGenerator::value_type;

    template<typename... Args>
    Cached(Args&&... args)
        : m_derived(std::forward<Args>(args)...) {
        makeCacheDirIfNeeded();
    }

    Optional<value_type> generate() {
        std::call_once(m_initFlag, [this]() {
            String cachePath = makeCacheFilePath();
            if (loadIfCached(cachePath)) {
                return;
            }

            generateAndCache(cachePath);
        });

        if (m_cachedValue) {
            return *m_cachedValue;
        }
        return std::nullopt;
    }

    uint64_t hashCode() const {
        return m_derived.hashCode();
    }

    String paramString() const {
        return m_derived.paramString();
    }

private:
    InnerGenerator m_derived;
    mutable Optional<typename InnerGenerator::value_type> m_cachedValue;
    mutable std::once_flag m_initFlag;

    void makeCacheDirIfNeeded() {
        std::filesystem::path cachePath{DEFAULT_CACHE_DIR};
        if (!std::filesystem::exists(cachePath)) {
            std::filesystem::create_directories(cachePath);
        }
    }

    String makeCacheFilePath() const {
        return fmt::format(
                "{}{}_{}.cache",
                DEFAULT_CACHE_DIR,
                m_derived.paramString(),
                std::to_string(m_derived.hashCode()));
    }

    bool loadIfCached(StringView cachePath) {
        if (!std::filesystem::exists(cachePath)) {
            return false;
        }

        size_t fileSize;
        Optional<Vector<uint8_t>> result =
                FileReader::s_instance->readFile(cachePath, fileSize);
        if (!result) {
            Logger::error(
                    "Cache file found. However, failed to read it: {}",
                    cachePath);
            return false;
        }

        Span<const uint8_t> dataSpan{
                result->data(),
                result->size()};
        auto deserializedValue = value_type::deserialize(dataSpan);

        if (!deserializedValue) {
            Logger::error(
                    "Failed to deserialize cached value from file: {}",
                    cachePath);
            return false;
        }

        Logger::info("Loaded cached value from file: {}", cachePath);
        m_cachedValue = *deserializedValue;
        return true;
    }

    bool generateAndCache(StringView cachePath) {
        m_cachedValue = m_derived.generate();
        if (!m_cachedValue) {
            moe::Logger::error(
                    "Failed to generate value for caching: {}",
                    m_derived.paramString());
            return false;
        }

        Vector<uint8_t> serializedData =
                m_cachedValue->serialize();

        std::ofstream outFile(
                cachePath.data(),
                std::ios::binary | std::ios::out);
        outFile.write(
                reinterpret_cast<const char*>(serializedData.data()),
                static_cast<std::streamsize>(serializedData.size()));
        outFile.close();

        Logger::info("Cached generated value to file: {}", cachePath);
        return true;
    }
};

MOE_END_NAMESPACE