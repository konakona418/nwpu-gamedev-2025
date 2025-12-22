#include "Core/FileWriter.hpp"

#include <fstream>

MOE_BEGIN_NAMESPACE

bool FileWriter::writeToFile(const moe::StringView filepath, const moe::StringView content) {
    std::ofstream ofs(filepath.data());
    if (!ofs.is_open()) {
        moe::Logger::error("FileWriter::writeToFile: failed to open file {}", filepath);
        return false;
    }

    moe::Logger::info("FileWriter::writeToFile: writing to file {}", filepath);

    ofs << content;
    return true;
}

bool FileWriter::writeToFile(const moe::StringView filepath, const moe::Span<uint8_t> content) {
    std::ofstream ofs(filepath.data(), std::ios::binary);
    if (!ofs.is_open()) {
        moe::Logger::error("FileWriter::writeToFile: failed to open file {}", filepath);
        return false;
    }

    moe::Logger::info("FileWriter::writeToFile: writing to file {}", filepath);

    ofs.write(reinterpret_cast<const char*>(content.data()), static_cast<std::streamsize>(content.size()));
    return true;
}

MOE_END_NAMESPACE