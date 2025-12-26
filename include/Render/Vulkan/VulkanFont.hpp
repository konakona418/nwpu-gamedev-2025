#pragma once

#include "Core/ResourceCache.hpp"
#include "Render/Vulkan/VulkanIdTypes.hpp"
#include "Render/Vulkan/VulkanSwapBuffer.hpp"
#include "Render/Vulkan/VulkanTypes.hpp"

#include <ft2build.h>
#include FT_FREETYPE_H

#include <utf8.h>

namespace moe {
    struct VulkanEngine;
};

namespace moe {
    struct VulkanFont {
    public:
        struct Character {
            glm::vec2 uvOffset;
            glm::vec2 pxOffset;
            glm::ivec2 size;
            glm::ivec2 bearing;
            uint32_t advance;
        };

        // initialize font binary once; creates an atlas for the initial size
        void init(VulkanEngine& engine, Span<uint8_t> fontData, float fontSize, StringView glyphRanges);

        // ! important: this must be called before loading any other size
        // ! otherwise segfault
        // create/add another size atlas on demand
        bool ensureSize(float fontSize, StringView glyphRanges);

        void destroy();

        // accessors per size (fontSize rounded to integer key)
        UnorderedMap<char32_t, Character>& getCharacters(float fontSize);

        ImageId getFontImageId(float fontSize) const;

        bool supportSize(float fontSize) const {
            uint32_t key = faceKey(fontSize);
            return m_faces.find(key) != m_faces.end();
        }

        bool lazyLoadCharacters(float fontSize);
        void addCharToLazyLoadQueue(char32_t c, float fontSize) {
            auto key = faceKey(fontSize);
            m_faces[key].pendingLazyLoadGlyphs.push_back(c);
        }

        struct FontImageBuffer {
            uint8_t* pixels{nullptr};
            bool manualCleanup{false};

            int widthInPixels{0};
            int heightInPixels{0};

            int glyphPerRow{0};
            int glyphPerColumn{0};

            int glyphCellSize{0};

            int maxGlyphCount{0};
            int currentGlyphCount{0};

            FontImageBuffer(int glyphCount, float fontSize) {
                glyphCellSize = static_cast<int>(fontSize) + CELL_PADDING * 2;// +2 for padding
                glyphPerRow = static_cast<int>(glm::ceil(glm::sqrt(static_cast<float>(glyphCount))));
                glyphPerColumn = static_cast<int>(glm::ceil(static_cast<float>(glyphCount) / static_cast<float>(glyphPerRow)));

                widthInPixels = glyphPerRow * glyphCellSize;
                heightInPixels = glyphPerColumn * glyphCellSize;

                maxGlyphCount = glyphPerRow * glyphPerColumn;
                currentGlyphCount = 0;

                pixels = new uint8_t[widthInPixels * heightInPixels];
                std::memset(pixels, 0, widthInPixels * heightInPixels);
            }

            bool addGlyph(uint8_t* glyphPixels, int glyphWidth, int glyphHeight, float* outUVOffsetX, float* outUVOffsetY) {
                if (currentGlyphCount >= maxGlyphCount) {
                    Logger::error("FontImageBuffer: Exceeded maximum glyph count");
                    return false;
                }

                if (glyphWidth > glyphCellSize - CELL_PADDING * 2 || glyphHeight > glyphCellSize - CELL_PADDING * 2) {
                    Logger::warn("Glyph size exceeds cell size: glyph ({}, {}), cell ({}, {})",
                                 glyphWidth, glyphHeight,
                                 glyphCellSize, glyphCellSize);

                    // clamp glyph size
                    glyphWidth = glm::min(glyphWidth, glyphCellSize - CELL_PADDING * 2);
                    glyphHeight = glm::min(glyphHeight, glyphCellSize - CELL_PADDING * 2);
                }

                int row = currentGlyphCount / glyphPerRow;
                int col = currentGlyphCount % glyphPerRow;

                *outUVOffsetX = static_cast<float>(col * glyphCellSize) / static_cast<float>(widthInPixels);
                *outUVOffsetY = static_cast<float>(row * glyphCellSize) / static_cast<float>(heightInPixels);

                for (int y = 0; y < glyphHeight; ++y) {
                    for (int x = 0; x < glyphWidth; ++x) {
                        int destX = col * glyphCellSize + x + CELL_PADDING;// +1 for padding
                        int destY = row * glyphCellSize + y + CELL_PADDING;

                        if (destX < 0 || destX >= widthInPixels || destY < 0 || destY >= heightInPixels) {
                            Logger::warn("Glyph destination out of bounds: destX={}, destY={}, width={}, height={}",
                                         destX, destY, widthInPixels, heightInPixels);
                            return false;
                        }

                        pixels[destY * widthInPixels + destX] = glyphPixels[y * glyphWidth + x];
                    }
                }

                currentGlyphCount++;

                return true;
            }

            struct CopyRange {
                size_t size;
                size_t offset;
            };

            CopyRange getCopyRange(int lastGlyphIndex) const {
                size_t lastRowBeginPx = std::floor(lastGlyphIndex / (float) glyphPerRow) * glyphCellSize;
                size_t currentRowEndPx = std::ceil(currentGlyphCount / (float) glyphPerRow) * glyphCellSize;

                size_t offset = lastRowBeginPx * widthInPixels;
                size_t size = (currentRowEndPx - lastRowBeginPx) * widthInPixels;
                return {size, offset};
            }

            uint8_t* leak() {
                manualCleanup = true;
                return pixels;
            }

            size_t sizeInBytes() const {
                return static_cast<size_t>(widthInPixels) * static_cast<size_t>(heightInPixels);
            }

            size_t usedSizeInBytes() const {
                return static_cast<size_t>(currentGlyphCount) * static_cast<size_t>(glyphCellSize) * static_cast<size_t>(glyphCellSize);
            }

            size_t getGlyphBeginDataOffset(int glyphIndex) const {
                int row = glyphIndex / glyphPerRow;
                int col = glyphIndex % glyphPerRow;

                size_t offsetX = col * glyphCellSize;
                size_t offsetY = row * glyphCellSize;

                return offsetY * widthInPixels + offsetX;
            }

            Pair<size_t, size_t> getGlyphOffset(int glyphIndex) const {
                int row = glyphIndex / glyphPerRow;
                int col = glyphIndex % glyphPerRow;

                size_t offsetX = col * glyphCellSize;
                size_t offsetY = row * glyphCellSize;

                return {offsetX, offsetY};
            }

            ~FontImageBuffer() {
                if (manualCleanup) {
                    return;
                }
                if (!pixels) {
                    return;
                }

                delete[] pixels;
                pixels = nullptr;
            }
        };

        struct MetricResult {
            float width{0.0f};
            float height{0.0f};
        };

        struct PerCharMetric {
            float advance{0.0f};
            float height{0.0f};
        };

        struct PerFace {
            float fontSize{0.0f};
            UniquePtr<FontImageBuffer> fontImageBufferCPU;
            VulkanSwapBuffer fontImageBufferGPU;
            ImageId fontImageId{NULL_IMAGE_ID};
            UnorderedMap<char32_t, Character> characters;
            Vector<char32_t> pendingLazyLoadGlyphs;
            UnorderedMap<char32_t, PerCharMetric> measuredMetrics;
        };

        MetricResult measureText(std::u32string_view text, float fontSize);

    private:
        static constexpr int32_t CELL_PADDING = 1;

        // helper to convert float fontSize into integer key (pixels)
        static uint32_t faceKey(float fontSize) {
            return static_cast<uint32_t>(std::lround(fontSize));
        }

        FT_Library m_ftLibrary{nullptr};
        FT_Face m_faceFT{nullptr};

        StringView m_path;
        VulkanEngine* m_engine{nullptr};
        bool m_initialized{false};

        float m_defaultFontSize{0.0f};

        // raw font binary kept once for all faces
        uint8_t* m_fontData{nullptr};
        size_t m_fontDataSize{0};

        // map keyed by rounded font size (pixel)
        UnorderedMap<uint32_t, PerFace> m_faces;

        bool loadFontInternal(PerFace& face, std::u32string_view glyphRanges32);
    };

    struct VulkanFontLoader {
        SharedResource<VulkanFont> operator()(VulkanEngine& engine, StringView path, Span<uint8_t> fontData, float fontSize, StringView glyphRanges) {
            auto font = std::make_shared<VulkanFont>();
            font->init(engine, fontData, fontSize, glyphRanges);
            return SharedResource<VulkanFont>(font);
        }

        SharedResource<VulkanFont> operator()(VulkanFont&& font) {
            return std::make_shared<VulkanFont>(std::forward<VulkanFont>(font));
        }
    };

    struct VulkanFontDeleter {
        void operator()(VulkanFont* font) {
            if (font) {
                font->destroy();
                delete font;
            }
        }
    };

    struct VulkanFontCache : public ResourceCache<FontId, VulkanFont, VulkanFontLoader, VulkanFontDeleter> {};
}// namespace moe