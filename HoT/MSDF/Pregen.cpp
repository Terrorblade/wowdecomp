#include "HoT/MSDF/Pregen.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <freetype/ftbbox.h>
#include <freetype/ftoutln.h>
#include <msdfgen.h>
#include <msdfgen-ext.h>
#include "Engine/Source/ConsoleBase/ConsoleVar.h"
#include "HoT/MSDF/Cache.h"
#include "HoT/MSDF/Console.h"
#include "HoT/MSDF/Font.h"
#include "HoT/MSDF/Utils.h"
#include "HoT/MSDF/Validator.h"

namespace HoT::MSDF::Pregen {

// Sleeps between work slices so the worker threads stay near the requested CPU share.
class Throttle {
    public:
    Throttle(double targetPercent) : m_targetUsage(std::clamp(targetPercent, 1.0, 100.0)), m_lastSleep(std::chrono::steady_clock::now()) {}

    void StartWork() {
        this->m_lastSleep = std::chrono::steady_clock::now();
    }

    void EndWork() {
        auto now = std::chrono::steady_clock::now();
        this->m_accumulatedWork += std::chrono::duration_cast<std::chrono::milliseconds>(now - this->m_lastSleep);

        if (this->m_accumulatedWork.count() >= 100) {
            int32_t sleepMs = static_cast<int32_t>(this->m_accumulatedWork.count() / (this->m_targetUsage / 100.0) - this->m_accumulatedWork.count());

            if (sleepMs > 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(sleepMs));
            }

            this->m_accumulatedWork = std::chrono::milliseconds(0);
        }

        this->m_lastSleep = std::chrono::steady_clock::now();
    }

    private:
    double m_targetUsage;
    std::chrono::steady_clock::time_point m_lastSleep;
    std::chrono::milliseconds m_accumulatedWork{ 0 };
};

struct PreGenRequest {
    FT_Face m_face = nullptr;
    const uint8_t* m_data = nullptr;
    size_t m_size = 0;
    FT_Long m_faceIndex = 0;
    std::string m_familyName;
    std::string m_styleName;
};

static std::vector<PreGenRequest> s_pendingRequests;

static std::string GameLocale() {
    CVar* locale = CVar::Lookup("locale");

    return locale && locale->m_stringValue.m_str ? locale->m_stringValue.m_str : std::string();
}

static void FlushStdin() {
    int c;

    while ((c = getchar()) != '\n' && c != EOF) {
    }
}

void RegisterForPreGen(FT_Face face, const uint8_t* data, size_t size, FT_Long faceIndex) {
    if (!face || !data || size == 0) {
        return;
    }

    std::string name = face->family_name ? face->family_name : "";
    std::string style = face->style_name ? face->style_name : "";

    for (PreGenRequest& request : s_pendingRequests) {
        if (request.m_faceIndex == faceIndex && request.m_familyName == name && request.m_styleName == style) {
            request.m_data = data;
            request.m_size = size;
            return;
        }
    }

    PreGenRequest request;
    request.m_face = face;
    request.m_data = data;
    request.m_size = size;
    request.m_faceIndex = faceIndex;
    request.m_familyName = name;
    request.m_styleName = style;
    s_pendingRequests.push_back(request);
}

static bool AcquirePreGenLock() {
    std::error_code ec;
    std::filesystem::create_directories(Cache::CACHE_DIR, ec);

    std::string path = (std::filesystem::path(Cache::CACHE_DIR) / "pregen.lock").string();
    return Console::AcquireLock(path);
}

static void ReleasePreGenLock() {
    Console::ReleaseLock();
}

static bool GenerateFont(const PreGenRequest& request) {
    std::string locale = GameLocale();

    uint32_t start = 0x0020;
    uint32_t end = 0x00FF;
    const char* rangeName = "Basic Latin / Extended ASCII";

    if (locale == "zhCN" || locale == "zhTW") {
        end = 0x9FFF;
        rangeName = "CJK Unified Ideographs (Chinese)";
    } else if (locale == "koKR") {
        end = 0xD7AF;
        rangeName = "Hangul Syllables (Korean)";
    } else if (locale == "ruRU") {
        end = 0x04FF;
        rangeName = "Cyrillic (Russian)";
    }

    printf("\n=== Generating: %s %s ===\n", request.m_familyName.c_str(), request.m_styleName.c_str());
    printf("Select Generation Depth:\n");
    printf("1. Standard %s (U+%04X - U+%04X)\n", rangeName, start, end);
    printf("2. Custom Range\n");
    printf("0. Exit\n");
    printf("Choice: ");

    int choice = 1;

    if (!Console::ReadInt(choice)) {
        FlushStdin();
        printf("ERROR: %s\n", "Invalid input");
        return false;
    }

    FlushStdin();

    if (choice == 0) {
        return false;
    }

    if (choice == 2) {
        printf("Enter Start Hex (e.g. 4E00): ");

        if (!Console::ReadHex(start)) {
            FlushStdin();
            printf("ERROR: %s\n", "Invalid input");
            return false;
        }

        FlushStdin();
        printf("Enter End Hex (e.g. 9FFF): ");

        if (!Console::ReadHex(end)) {
            FlushStdin();
            printf("ERROR: %s\n", "Invalid input");
            return false;
        }

        FlushStdin();
    }

    if (end < start) {
        printf("ERROR: End must be >= start\n");
        return false;
    }

    const uint64_t total64 = static_cast<uint64_t>(end) - start + 1;

    if (total64 > 0xFFFFFFFF) {
        printf("ERROR: Range too large (max 4 billion glyphs)\n");
        return false;
    }

    const uint32_t total = static_cast<uint32_t>(total64);

    double cpuLimit = 100.0;
    printf("\nEnter CPU usage limit (1-100%%, 100 for unlimited):\n");
    printf("Choice (%%): ");

    if (!Console::ReadDouble(cpuLimit)) {
        FlushStdin();
        printf("ERROR: Invalid input. Using 100%% (unlimited).\n");
        cpuLimit = 100.0;
    }

    FlushStdin();

    cpuLimit = std::clamp(cpuLimit, 1.0, 100.0);
    printf("CPU limit set to: %.0f%%\n", cpuLimit);
    printf("\nGenerating %u glyphs...\n", total);

    if (total == 0) {
        printf("ERROR: %s\n", "Nothing to generate");
        return false;
    }

    Cache cache(request.m_data, request.m_size, request.m_familyName.c_str(), request.m_styleName.c_str(), SDF_RENDER_SIZE, SDF_SPREAD);

    unsigned int numThreads = std::thread::hardware_concurrency();

    if (numThreads == 0) {
        numThreads = 4;
    }

    FT_Library ftLib = nullptr;

    if (FT_Init_FreeType(&ftLib) != 0 || !ftLib) {
        printf("ERROR: %s\n", "FT_Init_FreeType failed");
        return false;
    }

    std::vector<FT_Face> threadFaces(numThreads, nullptr);
    std::vector<msdfgen::FontHandle*> threadFonts(numThreads, nullptr);
    bool allHandlesValid = FreetypeLibrary() != nullptr;

    for (unsigned int i = 0; i < numThreads && allHandlesValid; i++) {
        if (FT_New_Memory_Face(ftLib, request.m_data, static_cast<FT_Long>(request.m_size), request.m_faceIndex, &threadFaces[i]) != 0) {
            printf("ERROR: FT_New_Memory_Face failed for %s\n", request.m_familyName.empty() ? "unknown font" : request.m_familyName.c_str());
            threadFaces[i] = nullptr;
            allHandlesValid = false;
            break;
        }

        threadFonts[i] = msdfgen::loadFontData(FreetypeLibrary(), request.m_data, static_cast<int>(request.m_size));

        if (!threadFonts[i]) {
            allHandlesValid = false;
        }
    }

    auto releaseHandles = [&]() {
        for (unsigned int i = 0; i < numThreads; i++) {
            if (threadFonts[i]) {
                msdfgen::destroyFont(threadFonts[i]);
                threadFonts[i] = nullptr;
            }

            if (threadFaces[i]) {
                FT_Done_Face(threadFaces[i]);
                threadFaces[i] = nullptr;
            }
        }

        FT_Done_FreeType(ftLib);
    };

    if (!allHandlesValid) {
        releaseHandles();
        printf("ERROR: %s\n", "Failed to create MSDFFont instances");
        return false;
    }

    std::atomic<uint32_t> nextCp(start);
    std::atomic<uint32_t> doneCount(0);
    std::atomic<bool> workerError(false);
    std::mutex cacheMutex;

    std::thread progressThread([&]() {
        while (!workerError.load(std::memory_order_acquire)) {
            uint32_t now = doneCount.load(std::memory_order_relaxed);

            if (now >= total) {
                break;
            }

            printf("\rProgress: %u/%u (%.1f%%)   ", now, total, static_cast<double>(now) / total * 100.0);
            fflush(stdout);
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
    });

    auto worker = [&](uint32_t workerId, msdfgen::FontHandle* font, FT_Face localFace) {
        if (FT_Set_Pixel_Sizes(localFace, SDF_RENDER_SIZE, SDF_RENDER_SIZE) != 0) {
            workerError.store(true, std::memory_order_release);
            printf("ERROR: FT_Set_Pixel_Sizes failed in worker %u\n", workerId);
            return;
        }

        Throttle throttle(cpuLimit);
        std::vector<uint8_t> msdfData = Pools::s_byte.Acquire(512 * 512 * 4);

        while (!workerError.load(std::memory_order_acquire)) {
            uint32_t cp = nextCp.fetch_add(1, std::memory_order_acq_rel);

            if (cp > end) {
                break;
            }

            throttle.StartWork();

            if (FT_Load_Glyph(localFace, FT_Get_Char_Index(localFace, cp), FT_LOAD_NO_BITMAP | FT_LOAD_NO_HINTING) != 0) {
                doneCount.fetch_add(1, std::memory_order_relaxed);
                throttle.EndWork();
                continue;
            }

            FT_GlyphSlot slot = localFace->glyph;
            bool hasOutline = slot->format == FT_GLYPH_FORMAT_OUTLINE && slot->outline.n_contours > 0;
            uint16_t width = 0;
            uint16_t height = 0;
            int16_t bitmapLeft = static_cast<int16_t>(slot->bitmap_left);
            int16_t bitmapTop = static_cast<int16_t>(slot->bitmap_top);

            msdfData.clear();

            if (hasOutline) {
                FT_BBox bbox;
                FT_Outline_Get_BBox(&slot->outline, &bbox);

                int32_t w = std::max(0, static_cast<int32_t>(((bbox.xMax + 63) >> 6) - (bbox.xMin >> 6)));
                int32_t h = std::max(0, static_cast<int32_t>(((bbox.yMax + 63) >> 6) - (bbox.yMin >> 6)));

                if (w > 0 && h > 0) {
                    int32_t sdfW = w + 2 * SDF_SPREAD;
                    int32_t sdfH = h + 2 * SDF_SPREAD;

                    if (sdfW <= 512 && sdfH <= 512 && Font::GenerateMSDF(msdfData, font, cp, sdfW, sdfH)) {
                        size_t expectedSize = static_cast<size_t>(sdfW) * sdfH * 4;

                        if (msdfData.size() == expectedSize) {
                            width = static_cast<uint16_t>(sdfW);
                            height = static_cast<uint16_t>(sdfH);
                        } else {
                            printf("WARNING: Glyph U+%04X size mismatch: got %zu, expected %zu\n", cp, msdfData.size(), expectedSize);
                        }
                    }
                }
            }

            throttle.EndWork();

            GlyphToStore glyph;
            glyph.m_codepoint = cp;
            glyph.m_width = width;
            glyph.m_height = height;
            glyph.m_bitmapLeft = bitmapLeft;
            glyph.m_bitmapTop = bitmapTop;
            glyph.m_pixels.assign(msdfData.begin(), msdfData.end());
            glyph.m_dataSize = static_cast<uint32_t>(glyph.m_pixels.size());

            {
                std::lock_guard<std::mutex> lock(cacheMutex);

                if (!cache.StoreGlyph(std::move(glyph))) {
                    printf("WARNING: Failed to store glyph U+%04X\n", cp);
                }
            }

            doneCount.fetch_add(1, std::memory_order_relaxed);
        }

        Pools::s_byte.Release(std::move(msdfData));
    };

    std::vector<std::thread> threads;
    threads.reserve(numThreads);

    for (unsigned int i = 0; i < numThreads; i++) {
        threads.emplace_back(worker, i, threadFonts[i], threadFaces[i]);
    }

    for (std::thread& thread : threads) {
        thread.join();
    }

    progressThread.join();

    printf("\rProgress: %u/%u (100.0%%)                      \n", doneCount.load(), total);
    printf("Writing to disk...");
    fflush(stdout);
    cache.FlushPendingWrites();
    printf(" Done.\n");

    releaseHandles();

    bool success = !workerError.load(std::memory_order_acquire);

    if (success) {
        printf("Generation complete. Press Enter to continue...");
    } else {
        printf("\nGeneration encountered errors. Press Enter to continue...");
    }

    getchar();

    return success;
}

static void ExecutePreGeneration() {
    Console::Session console;

    if (!console.IsOpen()) {
        return;
    }

    std::string locale = GameLocale();

    // Drop fonts the validator rejects, the DLL meant to and left them in the menu.
    s_pendingRequests.erase(std::remove_if(s_pendingRequests.begin(), s_pendingRequests.end(), [](const PreGenRequest& request) {
        if (!AllowUnsafeFonts() && FreetypeLibrary()) {
            msdfgen::FontHandle* font = msdfgen::loadFontData(FreetypeLibrary(), request.m_data, static_cast<int>(request.m_size));
            bool valid = Validator::IsFontCompatible(font);

            if (font) {
                msdfgen::destroyFont(font);
            }

            return !valid;
        }

        return false;
    }), s_pendingRequests.end());

    while (!s_pendingRequests.empty()) {
        printf("\n=== MSDF Font Pre-Generation ===\n");
        printf("Detected Locale: %s\n\n", locale.c_str());
        printf("Available Fonts (Current Progress):\n");

        for (size_t i = 0; i < s_pendingRequests.size(); i++) {
            const PreGenRequest& request = s_pendingRequests[i];
            printf("%zu. %s %s", i + 1, request.m_familyName.c_str(), request.m_styleName.c_str());

            Cache probe(nullptr, 0, request.m_familyName.c_str(), request.m_styleName.c_str(), SDF_RENDER_SIZE, SDF_SPREAD);
            size_t count = probe.GetManifestSize();

            if (count > 0) {
                printf(" (Cache found: %zu entries%s)", count, count >= CJK_CACHE_THRESHOLD ? " [CJK-READY]" : "");
            }

            printf("\n");
        }

        printf("\nOptions:\n");
        printf("0. Exit\n");
        printf("Select font number: ");

        int choice = 0;

        if (!Console::ReadInt(choice)) {
            FlushStdin();
            printf("ERROR: %s\n", "Invalid input");
            continue;
        }

        FlushStdin();

        if (choice == 0) {
            break;
        }

        if (choice < 1 || choice > static_cast<int>(s_pendingRequests.size())) {
            printf("ERROR: %s\n", "Invalid selection");
            continue;
        }

        GenerateFont(s_pendingRequests[choice - 1]);
    }
}

bool TryStartPreGen() {
    if (s_pendingRequests.empty()) {
        return false;
    }

    if (!AcquirePreGenLock()) {
        printf("Pre-generation already running in another instance.\n");
        return false;
    }

    ExecutePreGeneration();
    ReleasePreGenLock();

    return true;
}

void Shutdown() {
    s_pendingRequests.clear();
    ReleasePreGenLock();
}

}
