#include "graphics/TextureResidency.h"

#include "graphics/GraphicsContext.h"
#include "graphics/TextureArray.h"
#include "io/ImageData.h"
#include "utils/Log.h"

#include <algorithm>
#include <stdexcept>
#include <utility>

TextureResidency::TextureResidency(const GraphicsContext& context, const ImportSettings& settings)
    : context(context), settings(settings) {
}

TextureResidency::~TextureResidency() {
    // Pages own GL resources; destruction must happen while the context is current.
    ReleaseAll();
}

TextureLocation TextureResidency::Acquire(const DecodedImage& image) {
    context.RequireCurrent();
    const auto key = image.CacheKey();
    if(const auto found = entries.find(key); found != entries.end()) {
        auto& entry = found->second;
        if(entry.users == 0) {
            unusedBytes -= entry.bytes;
        }
        ++entry.users;
        entry.lastUse = ++useClock;
        return {entry.page->array.get(), entry.layer};
    }

    Page* page = FindPageWithSlot();
    bool created = false;
    if(page == nullptr) {
        Reclaim();
        page = CreatePage();
        created = true;
    }
    const std::uint32_t layer = AllocateLayer(*page);
    try {
        page->array->UploadLayer(layer, image);
    } catch(...) {
        // The entry is not committed: roll the slot allocation back and drop an empty new page.
        ReleaseLayer(*page, layer);
        if(created && page->usedLayers == 0) {
            FreePage(page);
        }
        throw;
    }
    Entry entry;
    entry.page = page;
    entry.layer = layer;
    entry.users = 1;
    entry.bytes = page->layerBytes;
    entry.lastUse = ++useClock;
    entries.emplace(key, entry);
    return {page->array.get(), layer};
}

void TextureResidency::Release(std::string_view cacheKey) {
    context.RequireCurrent();
    const auto found = entries.find(std::string(cacheKey));
    if(found == entries.end()) {
        return;
    }
    auto& entry = found->second;
    if(entry.users == 0) {
        return;
    }
    --entry.users;
    entry.lastUse = ++useClock;
    if(entry.users == 0) {
        unusedBytes += entry.bytes;
    }
    if(settings.limits.maxTextureBytes != 0 && residentBytes > settings.limits.maxTextureBytes) {
        Reclaim();
    }
}

TextureResidency::Page* TextureResidency::FindPageWithSlot() {
    for(auto& page : pages) {
        if(!page->freeLayers.empty() || page->nextLayer < page->capacity) {
            return page.get();
        }
    }
    return nullptr;
}

TextureResidency::Page* TextureResidency::CreatePage() {
    auto page = std::make_unique<Page>();
    page->capacity = static_cast<std::uint32_t>(settings.textureLayersPerPage);
    page->array = std::make_unique<TextureArray>(
        context, settings.textureSize, settings.textureSize, settings.textureLayersPerPage, true);
    page->pageBytes = page->array->GetBytes();
    page->layerBytes = page->capacity == 0 ? 0 : page->pageBytes / page->capacity;
    residentBytes += page->pageBytes;
    if(settings.limits.maxTextureBytes != 0 && residentBytes > settings.limits.maxTextureBytes &&
       !warnedOverBudget) {
        // Live assets cannot be evicted; report once rather than failing the import.
        Log(LogLevel::Warning,
            "Texture residency exceeds the configured budget; is a model set too large?");
        warnedOverBudget = true;
    }
    pages.push_back(std::move(page));
    return pages.back().get();
}

std::uint32_t TextureResidency::AllocateLayer(Page& page) {
    std::uint32_t layer = 0;
    if(!page.freeLayers.empty()) {
        layer = page.freeLayers.back();
        page.freeLayers.pop_back();
    }
    else {
        layer = page.nextLayer++;
    }
    ++page.usedLayers;
    return layer;
}

void TextureResidency::ReleaseLayer(Page& page, std::uint32_t layer) {
    page.freeLayers.push_back(layer);
    if(page.usedLayers > 0) {
        --page.usedLayers;
    }
}

void TextureResidency::FreeEntry(std::unordered_map<std::string, Entry>::iterator entry) {
    const Entry value = entry->second;
    entries.erase(entry);
    if(value.users == 0) {
        unusedBytes -= std::min(unusedBytes, value.bytes);
    }
    ReleaseLayer(*value.page, value.layer);
    if(value.page->usedLayers == 0) {
        FreePage(value.page);
    }
}

void TextureResidency::FreePage(Page* page) {
    const auto found = std::find_if(pages.begin(), pages.end(), [page](const auto& candidate) {
        return candidate.get() == page;
    });
    if(found == pages.end()) {
        return;
    }
    residentBytes -= std::min(residentBytes, (*found)->pageBytes);
    pages.erase(found);
}

void TextureResidency::Reclaim() {
    context.RequireCurrent();
    if(settings.limits.maxTextureBytes == 0) {
        return;
    }
    while(residentBytes > settings.limits.maxTextureBytes) {
        auto victim = entries.end();
        for(auto it = entries.begin(); it != entries.end(); ++it) {
            if(it->second.users != 0) {
                continue;
            }
            if(victim == entries.end() || it->second.lastUse < victim->second.lastUse) {
                victim = it;
            }
        }
        if(victim == entries.end()) {
            break; // Nothing evictable: every resident page is still referenced.
        }
        FreeEntry(victim);
    }
}

void TextureResidency::TrimUnused() {
    context.RequireCurrent();
    std::vector<std::string> unused;
    unused.reserve(entries.size());
    for(const auto& [key, entry] : entries) {
        if(entry.users == 0) {
            unused.push_back(key);
        }
    }
    for(const auto& key : unused) {
        const auto found = entries.find(key);
        if(found != entries.end() && found->second.users == 0) {
            FreeEntry(found);
        }
    }
}

void TextureResidency::FinalizeUploads() {
    context.RequireCurrent();
    for(auto& page : pages) {
        page->array->GenerateMipmaps();
    }
}

void TextureResidency::ReleaseAll() {
    entries.clear();
    pages.clear();
    residentBytes = 0;
    unusedBytes = 0;
}
