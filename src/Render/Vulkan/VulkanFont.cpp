#include "Render/Vulkan/VulkanFont.hpp"
#include "Render/Vulkan/VulkanEngine.hpp"

#include "Render/Vulkan/VulkanUtils.hpp"

#include "Math/Util.hpp"

#include <ft2build.h>
#include FT_FREETYPE_H

namespace moe {
    // Ascii and Latin
    constexpr uint16_t DEFAULT_GLYPH_CODEPOINT_RANGE = 255;

    std::u32string generateDefaultGlyphRange() {
        static std::u32string cached{};
        if (!cached.empty()) {
            return cached;
        }

        Vector<char32_t> ss;
        // ascii
        for (int i = 0; i < DEFAULT_GLYPH_CODEPOINT_RANGE; i++) {
            ss.push_back(static_cast<char32_t>(i));
        }
        cached = std::u32string(ss.begin(), ss.end());

        return cached;
    }

    static VulkanFont::PerFace createFaceTemplate(float fontSize, size_t glyphCount) {
        VulkanFont::PerFace f;
        f.fontSize = fontSize;
        f.fontImageBufferCPU = std::make_unique<VulkanFont::FontImageBuffer>(static_cast<int>(glyphCount), fontSize);
        return f;
    }

    bool VulkanFont::loadFontInternal(PerFace& face, std::u32string_view glyphRanges32) {
        FT_Library ft = m_ftLibrary;
        FT_Face faceFT = m_faceFT;

        if (FT_Set_Pixel_Sizes(faceFT, 0, static_cast<FT_UInt>(face.fontSize))) {
            Logger::error("Failed to set font pixel size");
            return false;
        }

        for (char32_t c: glyphRanges32) {
            if (c < DEFAULT_GLYPH_CODEPOINT_RANGE && !std::isprint(static_cast<char>(c))) {
                // not printable
                continue;
            }

            auto charGlyphIdx = FT_Get_Char_Index(faceFT, c);
            if (FT_Load_Glyph(faceFT, charGlyphIdx, FT_LOAD_RENDER)) {
                Logger::warn("Failed to load Glyph {}", static_cast<uint32_t>(c));
                continue;
            }

            if (FT_Render_Glyph(faceFT->glyph, FT_RENDER_MODE_NORMAL)) {
                Logger::warn("Failed to render Glyph {}", static_cast<uint32_t>(c));
                continue;
            }

            auto& charInfo = face.characters[c];
            bool result = face.fontImageBufferCPU->addGlyph(
                    faceFT->glyph->bitmap.buffer,
                    faceFT->glyph->bitmap.width,
                    faceFT->glyph->bitmap.rows,
                    &charInfo.uvOffset.x,
                    &charInfo.uvOffset.y);
            if (!result) {
                Logger::warn("Failed to add Glyph {} to font image buffer", static_cast<uint32_t>(c));
                continue;
            }

            charInfo.pxOffset = {
                    charInfo.uvOffset.x * face.fontImageBufferCPU->widthInPixels + CELL_PADDING,
                    charInfo.uvOffset.y * face.fontImageBufferCPU->heightInPixels + CELL_PADDING,
            };

            charInfo.size = glm::ivec2(faceFT->glyph->bitmap.width, faceFT->glyph->bitmap.rows);
            charInfo.bearing = glm::ivec2(faceFT->glyph->bitmap_left, faceFT->glyph->bitmap_top);
            charInfo.advance = static_cast<uint32_t>(faceFT->glyph->advance.x);

            face.characters.emplace(c, charInfo);
        }

        return true;
    }

    void VulkanFont::init(VulkanEngine& engine, Span<uint8_t> fontData, float fontSize, StringView glyphRanges) {
        MOE_ASSERT(!fontData.empty(), "Font data is empty");
        MOE_ASSERT(fontSize > 0.0f, "Font size must be greater than 0");
        MOE_ASSERT(!m_initialized, "Font is already initialized");

        m_engine = &engine;
        m_defaultFontSize = fontSize;

        m_fontData = new uint8_t[fontData.size()];
        m_fontDataSize = fontData.size();
        std::memcpy(m_fontData, fontData.data(), fontData.size());

        FT_Library ft;
        if (FT_Init_FreeType(&ft)) {
            Logger::error("Could not init FreeType Library");
            return;
        }
        m_ftLibrary = ft;

        FT_Face faceFT;
        if (FT_New_Memory_Face(ft, m_fontData, m_fontDataSize, 0, &faceFT)) {
            Logger::error("Failed to load font from memory");
            FT_Done_FreeType(ft);
            return;
        }
        m_faceFT = faceFT;

        U32String glyphRanges32;
        auto defaultGlyphRange = generateDefaultGlyphRange();
        if (glyphRanges.empty()) {
            glyphRanges32 = defaultGlyphRange;
        } else {
            glyphRanges32 = utf8::utf8to32(glyphRanges);
        }

        // create initial face
        uint32_t key = faceKey(fontSize);
        PerFace face = createFaceTemplate(fontSize, glyphRanges32.size());

        loadFontInternal(face, defaultGlyphRange);

        // create GPU image for this face
        face.fontImageBufferGPU.init(*m_engine,
                                     VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                     static_cast<size_t>(face.fontImageBufferCPU->widthInPixels * face.fontImageBufferCPU->heightInPixels),
                                     1);

        VulkanAllocatedImage image = engine.allocateImage(
                {
                        static_cast<uint32_t>(face.fontImageBufferCPU->widthInPixels),
                        static_cast<uint32_t>(face.fontImageBufferCPU->heightInPixels),
                        1,
                },
                VK_FORMAT_R8_UNORM,
                VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                false);

        ImageId fontImageId = engine.m_caches.imageCache.addImage(std::move(image));
        face.fontImageId = fontImageId;

        // store
        m_faces[key] = std::move(face);

        m_initialized = true;

        // upload atlas to GPU
        m_engine->immediateSubmit([this, key](VkCommandBuffer cmd) {
            auto& f = m_faces[key];
            f.fontImageBufferGPU.upload(cmd, f.fontImageBufferCPU->pixels, 0, f.fontImageBufferCPU->sizeInBytes());
            VkUtils::copyFromBufferToImage(
                    cmd,
                    f.fontImageBufferGPU.getBuffer().buffer,
                    m_engine->m_caches.imageCache.getImage(f.fontImageId)->image,
                    VkExtent3D{
                            static_cast<uint32_t>(f.fontImageBufferCPU->widthInPixels),
                            static_cast<uint32_t>(f.fontImageBufferCPU->heightInPixels),
                            1},
                    false);
        });

        Logger::debug(
                "Initialized font with {} glyphs, atlas size: {}x{} (size {:.1f})",
                m_faces[key].characters.size(),
                m_faces[key].fontImageBufferCPU->widthInPixels, m_faces[key].fontImageBufferCPU->heightInPixels,
                m_defaultFontSize);
    }

    bool VulkanFont::ensureSize(float fontSize, StringView glyphRanges) {
        uint32_t key = faceKey(fontSize);
        if (m_faces.find(key) != m_faces.end()) return true;

        U32String glyphRanges32;
        auto defaultGlyphRange = generateDefaultGlyphRange();
        if (glyphRanges.empty()) {
            glyphRanges32 = defaultGlyphRange;
        } else {
            glyphRanges32 = utf8::utf8to32(glyphRanges);
        }

        PerFace face = createFaceTemplate(fontSize, glyphRanges32.size());
        if (!loadFontInternal(face, defaultGlyphRange)) return false;

        // create GPU image for this face
        face.fontImageBufferGPU.init(*m_engine,
                                     VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                     static_cast<size_t>(face.fontImageBufferCPU->widthInPixels * face.fontImageBufferCPU->heightInPixels),
                                     1);

        VulkanAllocatedImage image = m_engine->allocateImage(
                {
                        static_cast<uint32_t>(face.fontImageBufferCPU->widthInPixels),
                        static_cast<uint32_t>(face.fontImageBufferCPU->heightInPixels),
                        1,
                },
                VK_FORMAT_R8_UNORM,
                VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                false);

        ImageId fontImageId = m_engine->m_caches.imageCache.addImage(std::move(image));
        face.fontImageId = fontImageId;

        m_faces[key] = std::move(face);

        // upload atlas to GPU
        m_engine->immediateSubmit([this, key](VkCommandBuffer cmd) {
            auto& f = m_faces[key];
            f.fontImageBufferGPU.upload(cmd, f.fontImageBufferCPU->pixels, 0, f.fontImageBufferCPU->sizeInBytes());
            VkUtils::copyFromBufferToImage(
                    cmd,
                    f.fontImageBufferGPU.getBuffer().buffer,
                    m_engine->m_caches.imageCache.getImage(f.fontImageId)->image,
                    VkExtent3D{
                            static_cast<uint32_t>(f.fontImageBufferCPU->widthInPixels),
                            static_cast<uint32_t>(f.fontImageBufferCPU->heightInPixels),
                            1},
                    false);
        });

        return true;
    }

    bool VulkanFont::lazyLoadCharacters(float fontSize) {
        uint32_t key = faceKey(fontSize);
        auto it = m_faces.find(key);
        if (it == m_faces.end()) return true;

        auto& face = it->second;
        if (face.pendingLazyLoadGlyphs.empty()) {
            return true;
        }

        Logger::debug("Lazy loading {} glyphs for font size {}", face.pendingLazyLoadGlyphs.size(), fontSize);
        std::u32string glyphRanges32(face.pendingLazyLoadGlyphs.begin(), face.pendingLazyLoadGlyphs.end());
        face.pendingLazyLoadGlyphs.clear();

        size_t originalGlyphCount = face.fontImageBufferCPU->currentGlyphCount;
        loadFontInternal(face, glyphRanges32);

        size_t newGlyphCount = face.fontImageBufferCPU->currentGlyphCount - originalGlyphCount;
        auto copyRange = face.fontImageBufferCPU->getCopyRange(static_cast<int>(originalGlyphCount));

        m_engine->immediateSubmit([=](VkCommandBuffer cmd) {
            auto& f = m_faces[key];
            f.fontImageBufferGPU.upload(
                    cmd,
                    f.fontImageBufferCPU->pixels + copyRange.offset,
                    0,
                    copyRange.size,
                    copyRange.offset);

            // here we copy:
            // |----------existing glyphs------------|
            // :-> copyRange.offset
            // |----existing glyphs---|--new glyphs--|
            // |----new glyphs---|--not initialized--|
            // :-> copyRange.offset + copyRange.size
            VkUtils::copyFromBufferToImage(
                    cmd,
                    f.fontImageBufferGPU.getBuffer().buffer,
                    m_engine->m_caches.imageCache.getImage(f.fontImageId)->image,
                    VkExtent3D{
                            static_cast<uint32_t>(f.fontImageBufferCPU->widthInPixels),
                            static_cast<uint32_t>(copyRange.size / f.fontImageBufferCPU->widthInPixels),
                            1},
                    VkOffset3D{
                            0,
                            static_cast<int32_t>(copyRange.offset / f.fontImageBufferCPU->widthInPixels),
                            0},
                    copyRange.offset,
                    0,
                    0);
        });

        return true;
    }

    void VulkanFont::destroy() {
        for (auto& kv: m_faces) {
            kv.second.fontImageBufferGPU.destroy();
            // CPU buffer will be freed by UniquePtr destructor
        }
        m_faces.clear();

        if (m_fontData) {
            delete[] m_fontData;
            m_fontData = nullptr;
            m_fontDataSize = 0;
        }

        FT_Done_Face(m_faceFT);
        FT_Done_FreeType(m_ftLibrary);
    }

    UnorderedMap<char32_t, VulkanFont::Character>& VulkanFont::getCharacters(float fontSize) {
        uint32_t key = faceKey(fontSize);
        return m_faces[key].characters;
    }

    ImageId VulkanFont::getFontImageId(float fontSize) const {
        uint32_t key = faceKey(fontSize);
        auto it = m_faces.find(key);
        if (it == m_faces.end()) return NULL_IMAGE_ID;
        return it->second.fontImageId;
    }

    VulkanFont::MetricResult VulkanFont::measureText(std::u32string_view text, float fontSize) {
        MetricResult result{0.0f, 0.0f};
        FT_Library ft = m_ftLibrary;
        FT_Face faceFT = m_faceFT;

        if (FT_Set_Pixel_Sizes(faceFT, 0, static_cast<FT_UInt>(fontSize))) {
            Logger::error("Failed to set font pixel size");
            return result;
        }

        auto faceIt = m_faces.find(faceKey(fontSize));
        // not found, try to create it
        if (faceIt == m_faces.end()) {
            ensureSize(fontSize, "");
            faceIt = m_faces.find(faceKey(fontSize));

            // though not expected, still check
            if (faceIt == m_faces.end()) {
                Logger::error("Failed to ensure font size {}", fontSize);
                return result;
            }
        }
        auto& perface = faceIt->second;

        float maxHeight = 0.0f;
        float penX = 0.0f;

        for (char32_t c: text) {
            if (c < DEFAULT_GLYPH_CODEPOINT_RANGE && !std::isprint(static_cast<char>(c))) {
                // not printable
                continue;
            }

            auto it = perface.measuredMetrics.find(c);
            if (it != perface.measuredMetrics.end()) {
                maxHeight = Math::max(maxHeight, it->second.height);
                penX += it->second.advance;
            } else {
                auto charGlyphIdx = FT_Get_Char_Index(faceFT, c);
                if (FT_Load_Glyph(faceFT, charGlyphIdx, FT_LOAD_NO_BITMAP)) {
                    Logger::warn("Failed to load Glyph {}", static_cast<uint32_t>(c));
                    continue;
                }

                PerCharMetric metric{};
                metric.advance = static_cast<float>(faceFT->glyph->advance.x >> 6);
                metric.height = static_cast<float>(faceFT->glyph->metrics.height >> 6);

                perface.measuredMetrics[c] = metric;
                maxHeight = Math::max(maxHeight, metric.height);
                penX += metric.advance;
            }

            auto charGlyphIdx = FT_Get_Char_Index(faceFT, c);
            if (FT_Load_Glyph(faceFT, charGlyphIdx, FT_LOAD_NO_BITMAP)) {
                Logger::warn("Failed to load Glyph {}", static_cast<uint32_t>(c));
                continue;
            }
        }

        result.width = penX;
        result.height = maxHeight;

        return result;
    }

}// namespace moe