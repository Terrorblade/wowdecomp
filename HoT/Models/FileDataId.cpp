#include "HoT/Models/FileDataId.h"
#include <cctype>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include "HoT/Core/Log.h"
#include "HoT/Models/Db2.h"

namespace HoT::Models::Fdid {

// Both path tables decode to { uint32 id, int32 path }.
struct PathRow {
    uint32_t m_id;
    int32_t m_path;
};

// HoT: the one non-id field holds the path, listing it lets WDC2 and later offsets resolve where the DLL left them raw.
static const uint32_t s_pathColumns[2] = { 0, 1 };

struct Tables {
    Db2Table m_tex { sizeof(PathRow) };
    Db2Table m_model { sizeof(PathRow) };
    std::unordered_map<std::string, uint32_t> m_modelIdByStem;
    std::once_flag m_onceTex;
    std::once_flag m_onceModel;
    std::once_flag m_onceStems;
    bool m_texReady = false;
    bool m_modelReady = false;
};

// Leaked on purpose since the warm thread can still be loading at teardown.
static Tables& T() {
    static Tables* s_tables = new Tables();
    return *s_tables;
}

static std::string NormalizeStem(const char* path) {
    std::string stem;

    for (const char* p = path; *p; p++) {
        char c = *p;
        stem.push_back(c == '/' ? '\\' : static_cast<char>(tolower(static_cast<unsigned char>(c))));
    }

    size_t dot = stem.find_last_of('.');
    size_t sep = stem.find_last_of('\\');

    if (dot != std::string::npos && (sep == std::string::npos || dot > sep)) {
        stem.resize(dot);
    }

    return stem;
}

static void LoadTexTable() {
    T().m_tex.SetStringColumns(s_pathColumns, 2);
    T().m_tex.Load("TextureFilePath.db2");
    T().m_texReady = T().m_tex.RowCount() != 0;

    if (!T().m_texReady) {
        LOG_WARN << "db2-fdid: TextureFilePath.db2 unavailable, texture FileDataID resolution disabled";
    }
}

static void LoadModelTable() {
    T().m_model.SetStringColumns(s_pathColumns, 2);
    T().m_model.Load("ModelFilePath.db2");
    T().m_modelReady = T().m_model.RowCount() != 0;

    if (!T().m_modelReady) {
        LOG_WARN << "db2-fdid: ModelFilePath.db2 unavailable, model FileDataID resolution disabled";
    }
}

static void EnsureModelTable() {
    std::call_once(T().m_onceModel, LoadModelTable);
}

static void BuildStemIndex() {
    if (!T().m_modelReady) {
        return;
    }

    T().m_modelIdByStem.reserve(T().m_model.RowCount());

    for (uint32_t i = 0; i < T().m_model.RowCount(); i++) {
        const PathRow* row = static_cast<const PathRow*>(T().m_model.RowByIndex(i));

        if (!row) {
            continue;
        }

        const char* path = T().m_model.Str(static_cast<uint32_t>(row->m_path));

        // The first spelling wins, rows that collapse to one stem only differ by extension.
        if (path && *path) {
            T().m_modelIdByStem.emplace(NormalizeStem(path), row->m_id);
        }
    }
}

// The tables never change after their one load and are never freed, so their strings serve as the returned paths.
static const char* LookupPath(const Db2Table& table, uint32_t fileDataId) {
    const PathRow* row = static_cast<const PathRow*>(table.RowById(static_cast<int32_t>(fileDataId)));
    const char* path = row ? table.Str(static_cast<uint32_t>(row->m_path)) : nullptr;

    return path && *path ? path : nullptr;
}

void EnsureLoaded() {
    std::call_once(T().m_onceTex, LoadTexTable);
}

void BeginWarm() {
    static std::once_flag s_started;

    std::call_once(s_started, [] {
        try {
            std::thread([] {
                EnsureLoaded();
            }).detach();
        } catch (...) {
            LOG_WARN << "db2-fdid: prefetch thread could not start, the table loads on first use";
        }
    });
}

const char* ResolveTexture(uint32_t fileDataId) {
    if (!fileDataId) {
        return nullptr;
    }

    EnsureLoaded();

    if (!T().m_texReady) {
        return nullptr;
    }

    return LookupPath(T().m_tex, fileDataId);
}

const char* ResolveModel(uint32_t fileDataId) {
    if (!fileDataId) {
        return nullptr;
    }

    EnsureLoaded();
    EnsureModelTable();

    const char* path = T().m_modelReady ? LookupPath(T().m_model, fileDataId) : nullptr;

    if (!path && T().m_texReady) {
        path = LookupPath(T().m_tex, fileDataId);
    }

    return path;
}

uint32_t ResolveModelId(const char* modelPath) {
    if (!modelPath || !*modelPath) {
        return 0;
    }

    EnsureModelTable();
    std::call_once(T().m_onceStems, BuildStemIndex);

    if (T().m_modelIdByStem.empty()) {
        return 0;
    }

    auto it = T().m_modelIdByStem.find(NormalizeStem(modelPath));
    return it == T().m_modelIdByStem.end() ? 0 : it->second;
}

}
