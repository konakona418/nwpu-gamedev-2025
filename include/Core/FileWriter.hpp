#pragma once

#include "Core/Common.hpp"
#include "Core/Meta/Feature.hpp"

MOE_BEGIN_NAMESPACE

struct FileWriter
    : public moe::Meta::NonCopyable<FileWriter>,
      public moe::Meta::Singleton<FileWriter> {
public:
    MOE_SINGLETON(FileWriter);

    static bool writeToFile(const moe::StringView filepath, const moe::StringView content);
    static bool writeToFile(const moe::StringView filepath, const moe::Span<uint8_t> content);
};

MOE_END_NAMESPACE