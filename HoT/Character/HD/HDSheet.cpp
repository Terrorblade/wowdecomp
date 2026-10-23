#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <list>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include "HoT/Character/HD/HDSheet.h"
#include "HoT/Character/HD/HDBlpImage.h"
#include "HoT/Character/HD/HDCharacters.h"
#include "HoT/Character/HD/HDManifest.h"
#include "HoT/Character/HD/HDSheetPool.h"
#include "Engine/Source/Gx/CGxDevice/CGxDevice.h"
#include "Engine/Source/Gx/CGxTexFlags.h"
#include "Engine/Source/Gx/Gx.h"
#include "Engine/Source/Model2/M2Model.h"
#include "Engine/Source/Services/Texture.h"
#include "Engine/Source/Storm/H/storm.h"
#include "HoT/Core/Log.h"
#include "WoW/Common/ComponentCore/CharacterComponent.h"
#include "WoW/Common/TextureCache.h"

namespace HoT::HD {

static const uint32_t SHEET_WIDTH = 1024;
static const uint32_t SHEET_HEIGHT = 512;
static const uint32_t BANDS = 8;
static const uint32_t PARALLEL_MIP_ROWS = 64;
static const uint32_t MIP_LEVELS = 11;
static const size_t SHEET_CACHE_MAX = 32;
static const double REPORT_MILLIS = 2.0;
static const int32_t BODY_SECTION_COUNT = 8;
static const int32_t ITEM_LAYER_COUNT = 7;
static const uint32_t FLAG_TEXTURES_DIRTY = 0x01;
static const uint32_t FLAG_REQUEST_QUEUED = 0x08;
static const uint32_t FLAG_ROBE = 0x20;

struct Rect {
    uint32_t m_x;
    uint32_t m_y;
    uint32_t m_w;
    uint32_t m_h;
};

static const Rect SECTION_RECTS[NUM_COMPONENT_SECTIONS] = {
    { 0, 0, 256, 128 },
    { 0, 128, 256, 128 },
    { 0, 256, 256, 64 },
    { 256, 0, 256, 128 },
    { 256, 128, 256, 64 },
    { 256, 192, 256, 128 },
    { 256, 320, 256, 128 },
    { 256, 448, 256, 64 },
    { 512, 0, 512, 512 },
    { 512, 0, 512, 512 },
};

struct SlotRange {
    int32_t m_first;
    int32_t m_end;
    int32_t m_firstRobe;
    int32_t m_endRobe;
};

static const SlotRange SECTION_SLOTS[BODY_SECTION_COUNT] = {
    { 0, 2, 0, 2 },
    { 0, 7, 0, 7 },
    { 0, 1, 0, 1 },
    { 0, 5, 0, 5 },
    { 0, 7, 0, 7 },
    { 0, 3, 1, 2 },
    { 0, 6, 1, 4 },
    { 0, 1, 0, 1 },
};

struct SheetData {
    std::vector<std::vector<uint32_t>> m_mips;
    std::vector<uint32_t> m_widths;
    std::vector<uint32_t> m_heights;
};

struct PasteOp {
    std::string m_path;
    Rect m_rect;
    bool m_blend;
};

struct Recipe {
    std::vector<PasteOp> m_ops;
    std::string m_key;
};

struct Job {
    Recipe m_recipe;
    std::shared_ptr<const SheetData> m_result;
    std::atomic<bool> m_done { false };
};

// Lives on CCharacterComponent::m_hdSheet and is only touched from the main thread.
struct SheetSlot {
    std::shared_ptr<const SheetData> m_ready;
    std::weak_ptr<const SheetData> m_uploaded;
    std::shared_ptr<Job> m_job;
    bool m_fresh = false;
};

struct CacheEntry {
    std::shared_ptr<const SheetData> m_sheet;
    std::list<std::string>::iterator m_order;
};

static std::mutex s_cacheMutex;
static std::unordered_map<std::string, CacheEntry> s_cache;
static std::list<std::string> s_cacheOrder;

static SheetSlot& SlotOf(CCharacterComponent* component) {
    if (!component->m_hdSheet) {
        component->m_hdSheet = new SheetSlot();
    }

    return *component->m_hdSheet;
}

static const char* EntryPath(const void* texture) {
    if (!texture) {
        return nullptr;
    }

    const char* path = static_cast<const CACHEENTRY*>(texture)->m_fileName;
    return *path ? path : nullptr;
}

static bool IsHdTexturePath(const char* path) {
    return SStrCmpI(path, "characterhd\\", 12) == 0;
}

static double Millis(std::chrono::steady_clock::time_point start) {
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
}

static inline uint32_t Blend(uint32_t dst, uint32_t src) {
    uint32_t a = src >> 24;

    if (a == 255) {
        return src | 0xFF000000u;
    }

    if (a == 0) {
        return dst;
    }

    uint32_t ia = 255 - a;
    uint32_t r = ((src >> 16 & 0xFF) * a + (dst >> 16 & 0xFF) * ia) / 255;
    uint32_t g = ((src >> 8 & 0xFF) * a + (dst >> 8 & 0xFF) * ia) / 255;
    uint32_t b = ((src & 0xFF) * a + (dst & 0xFF) * ia) / 255;

    return 0xFF000000u | (r << 16) | (g << 8) | b;
}

static inline uint32_t Sample(const BlpImage& img, float fx, float fy) {
    fx = std::clamp(fx, 0.0f, static_cast<float>(img.m_width - 1));
    fy = std::clamp(fy, 0.0f, static_cast<float>(img.m_height - 1));
    uint32_t x0 = static_cast<uint32_t>(fx);
    uint32_t y0 = static_cast<uint32_t>(fy);
    uint32_t x1 = std::min(x0 + 1, img.m_width - 1);
    uint32_t y1 = std::min(y0 + 1, img.m_height - 1);
    uint32_t tx = static_cast<uint32_t>((fx - static_cast<float>(x0)) * 256.0f);
    uint32_t ty = static_cast<uint32_t>((fy - static_cast<float>(y0)) * 256.0f);
    uint32_t p00 = img.At(x0, y0);
    uint32_t p10 = img.At(x1, y0);
    uint32_t p01 = img.At(x0, y1);
    uint32_t p11 = img.At(x1, y1);
    uint32_t out = 0;

    for (int32_t shift = 0; shift < 32; shift += 8) {
        uint32_t c00 = (p00 >> shift) & 0xFF;
        uint32_t c10 = (p10 >> shift) & 0xFF;
        uint32_t c01 = (p01 >> shift) & 0xFF;
        uint32_t c11 = (p11 >> shift) & 0xFF;
        uint32_t top = c00 * (256 - tx) + c10 * tx;
        uint32_t bottom = c01 * (256 - tx) + c11 * tx;
        uint32_t c = (top * (256 - ty) + bottom * ty) >> 16;
        out |= (c & 0xFF) << shift;
    }

    return out;
}

static void PasteRows(std::vector<uint32_t>& sheet, const Rect& rect, const BlpImage& img, bool blend, uint32_t y0, uint32_t y1) {
    uint32_t rowBegin = std::max(y0, rect.m_y);
    uint32_t rowEnd = std::min({ y1, rect.m_y + rect.m_h, SHEET_HEIGHT });
    uint32_t cols = std::min(rect.m_w, SHEET_WIDTH - std::min(rect.m_x, SHEET_WIDTH));

    if (rowBegin >= rowEnd || !cols) {
        return;
    }

    if (img.m_width == rect.m_w && img.m_height == rect.m_h) {
        for (uint32_t y = rowBegin; y < rowEnd; y++) {
            uint32_t* row = &sheet[y * SHEET_WIDTH + rect.m_x];
            const uint32_t* src = &img.m_pixels[(y - rect.m_y) * img.m_width];

            if (blend) {
                for (uint32_t x = 0; x < cols; x++) {
                    row[x] = Blend(row[x], src[x]);
                }
            } else {
                for (uint32_t x = 0; x < cols; x++) {
                    row[x] = src[x] | 0xFF000000u;
                }
            }
        }

        return;
    }

    float sx = static_cast<float>(img.m_width) / static_cast<float>(rect.m_w);
    float sy = static_cast<float>(img.m_height) / static_cast<float>(rect.m_h);

    for (uint32_t y = rowBegin; y < rowEnd; y++) {
        uint32_t* row = &sheet[y * SHEET_WIDTH + rect.m_x];
        float fy = (static_cast<float>(y - rect.m_y) + 0.5f) * sy - 0.5f;

        for (uint32_t x = 0; x < cols; x++) {
            uint32_t s = Sample(img, (static_cast<float>(x) + 0.5f) * sx - 0.5f, fy);
            row[x] = blend ? Blend(row[x], s) : (s | 0xFF000000u);
        }
    }
}

static void MipRows(const std::vector<uint32_t>& src, uint32_t w, uint32_t h, std::vector<uint32_t>& dst, uint32_t nw, uint32_t y0, uint32_t y1) {
    for (uint32_t y = y0; y < y1; y++) {
        uint32_t sy0 = std::min(y * 2, h - 1);
        uint32_t sy1 = std::min(y * 2 + 1, h - 1);

        for (uint32_t x = 0; x < nw; x++) {
            uint32_t sx0 = std::min(x * 2, w - 1);
            uint32_t sx1 = std::min(x * 2 + 1, w - 1);
            uint32_t p[4] = { src[sy0 * w + sx0], src[sy0 * w + sx1], src[sy1 * w + sx0], src[sy1 * w + sx1] };
            uint32_t out = 0;

            for (int32_t shift = 0; shift < 32; shift += 8) {
                uint32_t sum = 0;

                for (uint32_t v : p) {
                    sum += (v >> shift) & 0xFF;
                }

                out |= ((sum + 2) / 4) << shift;
            }

            dst[y * nw + x] = out;
        }
    }
}

static void BuildMips(SheetData& sheet, bool parallel) {
    sheet.m_mips.resize(1);
    sheet.m_mips.reserve(MIP_LEVELS);
    sheet.m_widths.assign(1, SHEET_WIDTH);
    sheet.m_heights.assign(1, SHEET_HEIGHT);
    uint32_t w = SHEET_WIDTH;
    uint32_t h = SHEET_HEIGHT;

    while (w > 1 || h > 1) {
        uint32_t nw = std::max(1u, w / 2);
        uint32_t nh = std::max(1u, h / 2);
        const std::vector<uint32_t>& src = sheet.m_mips.back();
        std::vector<uint32_t> dst(static_cast<size_t>(nw) * nh);

        if (parallel && nh >= PARALLEL_MIP_ROWS) {
            Pool::Group group;
            uint32_t rows = nh / BANDS;

            for (uint32_t b = 0; b < BANDS; b++) {
                uint32_t y0 = b * rows;
                uint32_t y1 = b + 1 == BANDS ? nh : y0 + rows;
                Pool::Submit([&src, w, h, &dst, nw, y0, y1] {
                    MipRows(src, w, h, dst, nw, y0, y1);
                }, &group, true);
            }

            Pool::Wait(group);
        } else {
            MipRows(src, w, h, dst, nw, 0, nh);
        }

        sheet.m_mips.push_back(std::move(dst));
        sheet.m_widths.push_back(nw);
        sheet.m_heights.push_back(nh);
        w = nw;
        h = nh;
    }
}

static const SheetData& Blank() {
    static const std::shared_ptr<const SheetData> s_blank = [] {
        auto sheet = std::make_shared<SheetData>();
        sheet->m_mips.emplace_back(static_cast<size_t>(SHEET_WIDTH) * SHEET_HEIGHT, 0xFF000000u);
        BuildMips(*sheet, false);
        return sheet;
    }();

    return *s_blank;
}

static Rect RectOf(int32_t rect) {
    if (rect >= 0 && rect < BODY_SECTION_COUNT) {
        return SECTION_RECTS[rect];
    }

    if (rect == Manifest::RECT_FACE) {
        return SECTION_RECTS[SECTION_HEAD_UPPER];
    }

    if (rect == Manifest::RECT_BODY) {
        return Rect { 0, 0, 512, 512 };
    }

    return Rect { 0, 0, SHEET_WIDTH, SHEET_HEIGHT };
}

static void AddOp(Recipe& recipe, const char* path, const Rect& rect, bool blend) {
    if (!path || !*path) {
        return;
    }

    recipe.m_ops.push_back(PasteOp { path, rect, blend });
}

static void AddItemOps(Recipe& recipe, const CCharacterComponent* component, int32_t section) {
    const CharacterSection& sectionData = component->m_section[section];
    uint32_t mask = sectionData.m_layerMask;
    bool robe = (component->m_flags & FLAG_ROBE) != 0;
    const SlotRange& range = SECTION_SLOTS[section];
    int32_t first = robe ? range.m_firstRobe : range.m_first;
    int32_t end = robe ? range.m_endRobe : range.m_end;
    const CharacterBaseVariation& underwear = component->m_baseVariation[VARIATION_UNDERWEAR];

    if (section == SECTION_TORSO_UPPER && !(mask & 7)) {
        AddOp(recipe, EntryPath(underwear.m_texture[1]), SECTION_RECTS[section], true);
    }

    if (section == SECTION_LEG_UPPER && (robe || !(mask & 3))) {
        AddOp(recipe, EntryPath(underwear.m_texture[0]), SECTION_RECTS[section], true);
    }

    for (int32_t slot = first; slot < end; slot++) {
        if (!(mask & (1u << slot))) {
            continue;
        }

        const void* entry = sectionData.m_layerTex[slot];

        if (!entry) {
            continue;
        }

        AddOp(recipe, static_cast<const CACHEENTRY*>(entry)->m_fileName, SECTION_RECTS[section], true);
    }
}

static bool ItemEntriesMissing(const CCharacterComponent* component) {
    for (int32_t section = 0; section < NUM_COMPONENT_SECTIONS; section++) {
        const CharacterSection& sectionData = component->m_section[section];

        for (int32_t slot = 0; slot < ITEM_LAYER_COUNT; slot++) {
            if (sectionData.m_layerItemDisplayId[slot] && !sectionData.m_layerTex[slot]) {
                return true;
            }
        }
    }

    return false;
}

static void EnsureItems(CCharacterComponent* component) {
    if (!ItemEntriesMissing(component)) {
        return;
    }

    static int32_t s_reports = 0;

    if (s_reports < 20) {
        s_reports++;
        LOG_INFO << "hd characters: item textures were released before the sheet composite (dirty mask " << component->m_dirtySections << ", skin colour " << component->m_data.m_preferences.m_skinID << "), reloading them";
    }

    component->m_dirtySections = 0xFFFFFFFF;
    component->ItemsLoaded(1);
}

static std::string KeyOf(const std::vector<PasteOp>& ops) {
    std::string key;
    char buf[48];

    for (const PasteOp& op : ops) {
        snprintf(buf, sizeof(buf), "|%u,%u,%u,%u,%d:", op.m_rect.m_x, op.m_rect.m_y, op.m_rect.m_w, op.m_rect.m_h, op.m_blend ? 1 : 0);
        key += buf;

        for (char c : op.m_path) {
            key += c == '/' ? '\\' : static_cast<char>(tolower(static_cast<unsigned char>(c)));
        }
    }

    return key;
}

static Recipe BuildRecipe(CCharacterComponent* component, bool quiet) {
    EnsureItems(component);
    Recipe recipe;
    Rect whole { 0, 0, SHEET_WIDTH, SHEET_HEIGHT };
    const char* skinPath = EntryPath(component->m_baseVariation[VARIATION_SKIN].m_texture[0]);

    if (!quiet) {
        if (!skinPath) {
            LOG_WARN << "hd characters: component has no skin texture, sheet stays black";
        } else if (!IsHdTexturePath(skinPath)) {
            LOG_WARN << "hd characters: skin fell back to the standard texture " << skinPath;
        }
    }

    AddOp(recipe, skinPath, whole, false);

    for (int32_t section = 0; section < BODY_SECTION_COUNT; section++) {
        AddItemOps(recipe, component, section);
    }

    if (const Manifest::Data* data = Manifest::Of(component)) {
        std::vector<const Manifest::LayerLine*> layers;
        Manifest::ResolveLayers(*data, Manifest::LookOf(component), layers);

        for (const Manifest::LayerLine* layer : layers) {
            AddOp(recipe, layer->m_path.c_str(), RectOf(layer->m_rect), true);
        }
    }

    recipe.m_key = KeyOf(recipe.m_ops);
    return recipe;
}

static std::shared_ptr<const SheetData> Compose(const Recipe& recipe, bool parallel) {
    std::unordered_map<std::string, std::shared_ptr<const BlpImage>> images;

    for (const PasteOp& op : recipe.m_ops) {
        images.emplace(op.m_path, nullptr);
    }

    if (parallel && images.size() > 1) {
        Pool::Group group;

        for (auto& entry : images) {
            auto* target = &entry;
            Pool::Submit([target] {
                target->second = LoadBlp(target->first.c_str());
            }, &group, true);
        }

        Pool::Wait(group);
    } else {
        for (auto& entry : images) {
            entry.second = LoadBlp(entry.first.c_str());
        }
    }

    auto sheet = std::make_shared<SheetData>();
    sheet->m_mips.reserve(MIP_LEVELS);
    sheet->m_mips.emplace_back(static_cast<size_t>(SHEET_WIDTH) * SHEET_HEIGHT, 0xFF000000u);
    std::vector<uint32_t>& base = sheet->m_mips[0];

    auto paste = [&](uint32_t y0, uint32_t y1) {
        for (const PasteOp& op : recipe.m_ops) {
            auto it = images.find(op.m_path);

            if (it != images.end() && it->second) {
                PasteRows(base, op.m_rect, *it->second, op.m_blend, y0, y1);
            }
        }
    };

    if (parallel) {
        Pool::Group group;
        uint32_t rows = SHEET_HEIGHT / BANDS;

        for (uint32_t b = 0; b < BANDS; b++) {
            uint32_t y0 = b * rows;
            uint32_t y1 = b + 1 == BANDS ? SHEET_HEIGHT : y0 + rows;
            Pool::Submit([&paste, y0, y1] {
                paste(y0, y1);
            }, &group, true);
        }

        Pool::Wait(group);
    } else {
        paste(0, SHEET_HEIGHT);
    }

    BuildMips(*sheet, parallel);
    return sheet;
}

static std::shared_ptr<const SheetData> CacheFind(const std::string& key) {
    std::lock_guard<std::mutex> lock(s_cacheMutex);
    auto it = s_cache.find(key);

    if (it == s_cache.end()) {
        return nullptr;
    }

    s_cacheOrder.splice(s_cacheOrder.begin(), s_cacheOrder, it->second.m_order);
    return it->second.m_sheet;
}

static void CacheInsert(const std::string& key, std::shared_ptr<const SheetData> sheet) {
    std::lock_guard<std::mutex> lock(s_cacheMutex);
    auto it = s_cache.find(key);

    if (it != s_cache.end()) {
        it->second.m_sheet = std::move(sheet);
        s_cacheOrder.splice(s_cacheOrder.begin(), s_cacheOrder, it->second.m_order);
        return;
    }

    s_cacheOrder.push_front(key);
    s_cache[key] = CacheEntry { std::move(sheet), s_cacheOrder.begin() };

    while (s_cache.size() > SHEET_CACHE_MAX && !s_cacheOrder.empty()) {
        s_cache.erase(s_cacheOrder.back());
        s_cacheOrder.pop_back();
    }
}

static void CompositeNow(CCharacterComponent* component) {
    Recipe recipe = BuildRecipe(component, false);
    std::shared_ptr<const SheetData> sheet = CacheFind(recipe.m_key);

    if (!sheet) {
        auto start = std::chrono::steady_clock::now();
        sheet = Compose(recipe, true);
        CacheInsert(recipe.m_key, sheet);
        double ms = Millis(start);

        if (ms >= REPORT_MILLIS) {
            LOG_INFO << "hd characters: sheet composed on the main thread in " << ms << " ms (" << recipe.m_ops.size() << " pastes, " << Pool::ThreadCount() << " helper threads)";
        } else {
            LOG_DEBUG << "hd characters: sheet composed on the main thread in " << ms << " ms";
        }
    }

    SheetSlot& slot = SlotOf(component);
    slot.m_job.reset();
    slot.m_ready = std::move(sheet);
    slot.m_fresh = true;
}

static void StartJob(SheetSlot& slot, Recipe&& recipe) {
    auto job = std::make_shared<Job>();
    job->m_recipe = std::move(recipe);
    slot.m_job = job;

    Pool::Submit([job] {
        auto start = std::chrono::steady_clock::now();
        std::shared_ptr<const SheetData> sheet = Compose(job->m_recipe, false);
        CacheInsert(job->m_recipe.m_key, sheet);
        job->m_result = std::move(sheet);
        job->m_done.store(true, std::memory_order_release);
        LOG_DEBUG << "hd characters: sheet composed on a worker thread in " << Millis(start) << " ms";
    }, nullptr, false);
}

static bool Pin(CCharacterComponent* component) {
    SheetSlot& slot = SlotOf(component);

    if (!slot.m_ready) {
        slot.m_ready = slot.m_uploaded.lock();
    }

    return slot.m_ready != nullptr;
}

static void Unpin(CCharacterComponent* component) {
    SheetSlot* slot = component->m_hdSheet;

    if (!slot || !slot->m_ready) {
        return;
    }

    slot->m_uploaded = slot->m_ready;
    slot->m_ready.reset();
    slot->m_fresh = false;
}

static void Prepare(CCharacterComponent* component) {
    SheetSlot* slot = component->m_hdSheet;

    if (!slot || !slot->m_fresh || !slot->m_ready) {
        CompositeNow(component);
    }

    SlotOf(component).m_fresh = false;
}

static void Upload(CCharacterComponent* component) {
    CTexture* texture = component->m_baseSkinTexture;

    if (!texture) {
        return;
    }

    CGxTex* gxTex = TextureGetGxTex(texture, 1, nullptr);

    if (!gxTex) {
        return;
    }

    GxTexUpdate(gxTex, 0, 0, static_cast<int32_t>(SHEET_WIDTH), static_cast<int32_t>(SHEET_HEIGHT), 1);
    Unpin(component);
}

// CCharacterComponent::RenderPrepAll with the section pastes replaced by the HD composite.
static void RenderPrepAll(CCharacterComponent* component) {
    component->m_flags |= FLAG_TEXTURES_DIRTY;

    if (component->m_request) {
        component->m_request->m_flags &= ~0x1;
        component->m_request = nullptr;
    }

    component->m_dirtySections = 0xFFFFFFFF;
    component->m_flags &= ~FLAG_REQUEST_QUEUED;
    component->VariationsLoaded(1);
    component->ItemsLoaded(1);
    CompositeNow(component);
    component->m_flags &= ~FLAG_TEXTURES_DIRTY;
    component->m_dirtySections = 0;
    component->DestroyRenderTextures();
    component->m_link.Unlink();
}

static void SheetTextureCallback(EGxTexCommand command, uint32_t width, uint32_t height, uint32_t depth, uint32_t mipLevel, void* userArg, uint32_t& texelStrideInBytes, const void*& texels) {
    CCharacterComponent* component = static_cast<CCharacterComponent*>(userArg);

    switch (command) {
        case GxTex_Lock: {
            if (CCharacterComponent::s_bInRenderPrep) {
                break;
            }

            if (!Pin(component)) {
                RenderPrepAll(component);
            }

            break;
        }

        case GxTex_Latch: {
            Pin(component);
            SheetSlot* slot = component->m_hdSheet;
            const SheetData& sheet = slot && slot->m_ready ? *slot->m_ready : Blank();
            size_t level = std::min<size_t>(mipLevel, sheet.m_mips.size() - 1);
            texelStrideInBytes = sheet.m_widths[level] * 4;
            texels = sheet.m_mips[level].data();
            break;
        }

        case GxTex_Unlock: {
            if (!CCharacterComponent::s_bInRenderPrep) {
                Unpin(component);
            }

            break;
        }

        case GxTex_Recreate: {
            CCharacterComponent::UpdateBaseTexture(command, width, height, depth, mipLevel, userArg, texelStrideInBytes, texels);
            SheetSlot* slot = component->m_hdSheet;

            if (slot) {
                slot->m_ready.reset();
                slot->m_uploaded.reset();
                slot->m_fresh = false;
            }

            break;
        }

        default: {
            break;
        }
    }
}

static bool HasGxContext() {
    return g_theGxDevicePtr && g_theGxDevicePtr->CapsHasContext(-1) != 0;
}

bool CreateBaseTexture(CCharacterComponent* component) {
    if (!component->m_hd) {
        return false;
    }

    CGxTexFlags flags(GxTex_LinearMipLinear, 0, 0, 0, 0, 0, 1);

    if (g_theGxDevicePtr && g_theGxDevicePtr->DeviceApi() == GxApi_GLL) {
        flags.m_bit14 = 1;
    }

    CTexture* texture = TextureCreate(SHEET_WIDTH, SHEET_HEIGHT, GxTex_Argb8888, GxTex_Argb8888, flags, component, &SheetTextureCallback, "CharacterBaseSkinHD", 1);
    component->m_baseSkinTexture = texture;

    if (component->m_data.m_model && texture) {
        component->m_data.m_model->ReplaceTexture(1, texture);
    }

    return true;
}

bool PrepSections(CCharacterComponent* component) {
    if (!component->m_hd) {
        return false;
    }

    Prepare(component);
    Upload(component);
    ApplySlots(component);

    return true;
}

bool RequestSheet(CCharacterComponent* component) {
    if (!component->m_hd) {
        return false;
    }

    if (!HasGxContext()) {
        return true;
    }

    SheetSlot& slot = SlotOf(component);
    Recipe recipe = BuildRecipe(component, slot.m_job != nullptr);
    std::shared_ptr<const SheetData> sheet;

    if (slot.m_job) {
        if (slot.m_job->m_recipe.m_key != recipe.m_key) {
            slot.m_job.reset();
        } else if (!slot.m_job->m_done.load(std::memory_order_acquire)) {
            return true;
        } else {
            sheet = slot.m_job->m_result;
            slot.m_job.reset();
        }
    }

    if (!sheet) {
        sheet = CacheFind(recipe.m_key);
    }

    if (!sheet) {
        StartJob(slot, std::move(recipe));
        return true;
    }

    slot.m_ready = std::move(sheet);
    slot.m_fresh = true;
    component->RenderPrepSections();
    component->m_link.Unlink();

    return true;
}

namespace Sheet {

void Destroy(CCharacterComponent* component) {
    delete component->m_hdSheet;
    component->m_hdSheet = nullptr;
}

void ClearCache() {
    std::lock_guard<std::mutex> lock(s_cacheMutex);
    s_cache.clear();
    s_cacheOrder.clear();
}

}

}
