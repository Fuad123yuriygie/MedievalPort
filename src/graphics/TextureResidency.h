#pragma once

#include "core/Settings.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

class GraphicsContext;
class TextureArray;
struct DecodedImage;

// A resolved texture slice. The owning residency cache keeps the page alive while the location is
// referenced; callers must balance every Acquire with a Release.
struct TextureLocation {
    const TextureArray* textureArray = nullptr;
    std::uint32_t layer = 0;
};

// Tracks texture usage independently of scene instances. Images are deduplicated by content
// fingerprint, layers are recycled from a per-page free list, residency is bounded by a byte budget
// with least-recently-used eviction, and completely unused pages are released on the context thread.
class TextureResidency {
public:
    TextureResidency(const GraphicsContext& context, const ImportSettings& settings);
    ~TextureResidency();
    TextureResidency(const TextureResidency&) = delete;
    TextureResidency& operator=(const TextureResidency&) = delete;

    // Uploads the image into a free layer (or reuses an existing identical image) and takes one
    // reference. The cache entry is committed only if the upload succeeds.
    TextureLocation Acquire(const DecodedImage& image);
    // Drops one reference identified by DecodedImage::CacheKey.
    void Release(std::string_view cacheKey);

    // Enforces the residency budget by evicting least-recently-used unused entries.
    void Reclaim();
    // Frees every currently unused entry, plus any page left empty.
    void TrimUnused();
    // Generates mipmaps once for each page that received a layer since the previous call.
    void FinalizeUploads();
    void ReleaseAll();

    std::size_t ResidentBytes() const {
        return residentBytes;
    }
    std::size_t UnusedBytes() const {
        return unusedBytes;
    }

private:
    struct Page {
        std::unique_ptr<TextureArray> array;
        std::vector<std::uint32_t> freeLayers;
        std::uint32_t nextLayer = 0;
        std::uint32_t capacity = 0;
        std::uint32_t usedLayers = 0;
        std::size_t pageBytes = 0;
        std::size_t layerBytes = 0;
    };
    struct Entry {
        Page* page = nullptr;
        std::uint32_t layer = 0;
        std::uint32_t users = 0;
        std::size_t bytes = 0;
        std::uint64_t lastUse = 0;
    };

    Page* FindPageWithSlot();
    Page* CreatePage();
    std::uint32_t AllocateLayer(Page& page);
    void ReleaseLayer(Page& page, std::uint32_t layer);
    void FreeEntry(std::unordered_map<std::string, Entry>::iterator entry);
    void FreePage(Page* page);

    const GraphicsContext& context;
    ImportSettings settings;
    std::vector<std::unique_ptr<Page>> pages;
    std::unordered_map<std::string, Entry> entries;
    std::size_t residentBytes = 0;
    std::size_t unusedBytes = 0;
    std::uint64_t useClock = 0;
    bool warnedOverBudget = false;
};
