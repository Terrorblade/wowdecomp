// Renders every spell description with the client's own parser, no window or server, and applies them to item_spell_gem_desc.
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <vector>
#include <mysql.h>
#include "CachingSha2Plugin.h"
#include "Engine/Source/BlizzardCore/SFile2-Core.h"
#include "Engine/Source/BlizzardCore/Source/System/Memory/MemoryStorm.h"
#include "Engine/Source/FrameScript/FrameScript.h"
#include "Engine/Source/Os/Time.h"
#include "Engine/Source/Storm/H/storm.h"
#include "HoT/Spells/DescriptionVars.h"
#include "WoW/Source/DB/DBClient/DBClient.h"
#include "WoW/Source/Ui/QuestTextParser.h"

static bool ReadWholeFile(const std::string& path, std::string& data) {
    std::ifstream file(path, std::ios::binary);

    if (!file) {
        return false;
    }

    std::ostringstream stream;
    stream << file.rdbuf();
    data = stream.str();
    return true;
}

static std::string XmlValue(const std::string& xml, const char* tag) {
    std::string open = std::string("<") + tag + ">";
    std::string close = std::string("</") + tag + ">";
    size_t start = xml.find(open);

    if (start == std::string::npos) {
        return {};
    }

    start += open.size();
    size_t end = xml.find(close, start);

    if (end == std::string::npos) {
        return {};
    }

    std::string value = xml.substr(start, end - start);

    for (size_t at; (at = value.find("&amp;")) != std::string::npos;) {
        value.replace(at, 5, "&");
    }

    return value;
}

static std::string StripXmlComments(std::string xml) {
    for (size_t start; (start = xml.find("<!--")) != std::string::npos;) {
        size_t end = xml.find("-->", start);
        xml.erase(start, end == std::string::npos ? std::string::npos : end + 3 - start);
    }

    return xml;
}

static std::string ParentDirectory(std::string path) {
    while (!path.empty() && (path.back() == '\\' || path.back() == '/')) {
        path.pop_back();
    }

    size_t slash = path.find_last_of("\\/");
    return slash == std::string::npos ? std::string(".") : path.substr(0, slash);
}

// The stock loader opens through SFile, this feeds it the loose file from memory instead.
template <class T, class Table>
static bool LoadTable(Table& table, const std::string& directory) {
    const char* name = T::GetFilename();
    const char* base = strrchr(name, '\\');
    std::string path = directory + "/" + (base ? base + 1 : name);
    std::string data;

    if (!ReadWholeFile(path, data) || data.size() < 20) {
        fprintf(stderr, "cannot read %s\n", path.c_str());
        return false;
    }

    SFileImpl* f = new (0u, __FILE__, __LINE__) SFileImpl(SFILE_MEMORY);
    f->m_size = static_cast<uint32_t>(data.size());
    f->m_data = static_cast<uint8_t*>(Blizzard::Memory::Allocate(f->m_size, 0, __FILE__, __LINE__));
    f->m_position = 0;
    memcpy(f->m_data, data.data(), data.size());

    uint32_t header[5];
    SFileReadFile(f, header, sizeof(header), nullptr, nullptr, 0);

    if (header[0] != 'CBDW' || header[2] != static_cast<uint32_t>(T::s_numColumns) || header[3] != static_cast<uint32_t>(T::s_rowSize)) {
        fprintf(stderr, "%s: expected %d columns of %d bytes, found %u of %u\n", path.c_str(), T::s_numColumns, T::s_rowSize, header[2], header[3]);
        SFileCloseFile(f);
        return false;
    }

    table.m_numRecords = static_cast<int32_t>(header[1]);
    table.m_strings = static_cast<char*>(SMemAlloc(header[4] + 1, __FILE__, __LINE__, 0));
    table.m_maxID = 0;
    table.m_minID = 0xFFFFFFF;

    if (table.m_numRecords) {
        table.LoadRecords(f, __FILE__, __LINE__);
    }

    SFileReadFile(f, const_cast<char*>(table.m_strings), header[4], nullptr, nullptr, 0);
    SFileCloseFile(f);
    table.m_loaded = 1;
    return true;
}

static bool IsGenderToken(const char* text) {
    const char* c = text;

    while (isalpha(static_cast<uint8_t>(*c)) || *c == ' ') {
        c++;
    }

    if (c == text || *c != ':') {
        return false;
    }

    const char* female = ++c;

    while (isalpha(static_cast<uint8_t>(*c)) || *c == ' ') {
        c++;
    }

    return c != female && *c == ';';
}

// The UI's font strings resolve |4one:many; plurals and colour codes, external readers get plain text.
static std::string ResolveMarkup(const char* text) {
    std::string out;
    int32_t lastNumber = 2;

    for (const char* c = text; *c; c++) {
        if (*c >= '0' && *c <= '9') {
            const char* start = c;

            while (c[1] >= '0' && c[1] <= '9') {
                c++;
            }

            lastNumber = (c - start == 0 && *start == '1' && (start == text || start[-1] != '.')) ? 1 : 2;
            out.append(start, c + 1 - start);
            continue;
        }

        // $gmale:female; needs a character, the parser leaves it as $male:female;.
        if (*c == '$' && IsGenderToken(c + 1)) {
            const char* colon = strchr(c, ':');
            const char* semicolon = strchr(colon, ';');
            out.append(c + 1, colon - c - 1);
            out += '/';
            out.append(colon + 1, semicolon - colon - 1);
            c = semicolon;
            continue;
        }

        if (*c != '|') {
            out += *c;
            continue;
        }

        char code = c[1];

        if (code == '|') {
            out += '|';
            c++;
        } else if (code == 'c' && strlen(c) >= 10) {
            c += 9;
        } else if (code == 'r') {
            c++;
        } else if (code == '4') {
            const char* colon = strchr(c + 2, ':');
            const char* semicolon = colon ? strchr(colon + 1, ';') : nullptr;

            if (!semicolon) {
                out += *c;
                continue;
            }

            if (lastNumber == 1) {
                out.append(c + 2, colon - c - 2);
            } else {
                out.append(colon + 1, semicolon - colon - 1);
            }

            c = semicolon;
        } else {
            out += *c;
        }
    }

    return out;
}

static void AppendSqlString(std::string& out, const std::string& text) {
    out += '\'';

    for (char c : text) {
        switch (c) {
            case '\'': out += "\\'"; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            default: out += c; break;
        }
    }

    out += '\'';
}

struct DbConfig {
    std::string host;
    std::string user;
    std::string password;
    std::string database;
    uint32_t port = 3306;
};

static bool Query(MYSQL* mysql, const std::string& sql) {
    if (mysql_real_query(mysql, sql.data(), static_cast<unsigned long>(sql.size()))) {
        fprintf(stderr, "query failed: %s\n", mysql_error(mysql));
        return false;
    }

    return true;
}

// MySQL 8 accounts authenticate through caching_sha2_password, a connector plugin DLL, written out from the exe.
static std::string ExtractSha2Plugin() {
    std::error_code error;
    std::filesystem::path directory = std::filesystem::temp_directory_path(error) / "SpellTextExport";
    std::filesystem::create_directories(directory, error);
    std::filesystem::path path = directory / "caching_sha2_password.dll";

    if (std::filesystem::file_size(path, error) != sizeof(s_sha2Plugin)) {
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        file.write(reinterpret_cast<const char*>(s_sha2Plugin), sizeof(s_sha2Plugin));
    }

    return directory.generic_string();
}

static MYSQL* Connect(const DbConfig& db) {
    MYSQL* mysql = mysql_init(nullptr);
    mysql_options(mysql, MYSQL_SET_CHARSET_NAME, "utf8mb4");

    std::string pluginDirectory = ExtractSha2Plugin();
    mysql_options(mysql, MYSQL_PLUGIN_DIR, pluginDirectory.c_str());

    if (!mysql_real_connect(mysql, db.host.c_str(), db.user.c_str(), db.password.c_str(), db.database.c_str(), db.port, nullptr, 0)) {
        fprintf(stderr, "cannot connect to %s@%s:%u/%s: %s\n", db.user.c_str(), db.host.c_str(), db.port, db.database.c_str(), mysql_error(mysql));
        mysql_close(mysql);
        return nullptr;
    }

    return mysql;
}

static bool LoadExisting(MYSQL* mysql, std::map<int32_t, std::string>& rows) {
    if (!Query(mysql, "SELECT `SpellId`, `Description` FROM `item_spell_gem_desc`")) {
        return false;
    }

    MYSQL_RES* result = mysql_use_result(mysql);

    if (!result) {
        fprintf(stderr, "cannot read item_spell_gem_desc: %s\n", mysql_error(mysql));
        return false;
    }

    while (MYSQL_ROW row = mysql_fetch_row(result)) {
        unsigned long* lengths = mysql_fetch_lengths(result);
        rows[atoi(row[0])].assign(row[1] ? row[1] : "", row[1] ? lengths[1] : 0);
    }

    mysql_free_result(result);
    return true;
}

// Writes only the rows that differ, all or nothing.
static bool ApplyRows(MYSQL* mysql, const std::vector<std::pair<int32_t, std::string>>& rows) {
    static constexpr size_t kRowsPerStatement = 500;

    if (!Query(mysql, "START TRANSACTION")) {
        return false;
    }

    std::string sql;
    std::string escaped;

    for (size_t i = 0; i < rows.size(); i++) {
        sql += (i % kRowsPerStatement) ? "," : "REPLACE INTO `item_spell_gem_desc` (`SpellId`, `Description`) VALUES ";
        escaped.resize(rows[i].second.size() * 2 + 1);
        escaped.resize(mysql_real_escape_string(mysql, &escaped[0], rows[i].second.data(), static_cast<unsigned long>(rows[i].second.size())));
        sql += "(" + std::to_string(rows[i].first) + ",'" + escaped + "')";

        if ((i + 1) % kRowsPerStatement == 0 || i + 1 == rows.size()) {
            if (!Query(mysql, sql)) {
                Query(mysql, "ROLLBACK");
                return false;
            }

            sql.clear();
        }
    }

    return Query(mysql, "COMMIT");
}

static bool WriteSqlFile(const std::string& path, const std::vector<std::pair<int32_t, std::string>>& rows) {
    FILE* file = fopen(path.c_str(), "wb");

    if (!file) {
        fprintf(stderr, "cannot write %s\n", path.c_str());
        return false;
    }

    std::string sql;

    for (size_t i = 0; i < rows.size(); i++) {
        sql += (i % 500) ? ",\n(" : "REPLACE INTO `item_spell_gem_desc` (`SpellId`, `Description`) VALUES\n(";
        sql += std::to_string(rows[i].first) + ", ";
        AppendSqlString(sql, rows[i].second);
        sql += ')';

        if ((i + 1) % 500 == 0 || i + 1 == rows.size()) {
            sql += ";\n";
        }
    }

    fwrite(sql.data(), 1, sql.size(), file);
    fclose(file);
    return true;
}

static const char* kUsage = "usage: SpellTextExport [config.xml] [-d database] [-n] [-o out.sql] [-l level] [-g GlobalStrings.lua]\n"
                            "  applies the changed rows to item_spell_gem_desc over the config's MySQL connection\n"
                            "  -d  database holding the table (default new_world)\n"
                            "  -n  dry run, report what would change\n"
                            "  -o  write every row to a SQL file instead of touching the database\n"
                            "  -l  level to render at (default 80)\n";

int main(int argc, char** argv) {
    std::string configPath = "config.xml";
    std::string outputPath;
    std::string globalStringsPath;
    std::string database = "new_world";
    int32_t level = 80;
    bool dryRun = false;

    for (int32_t i = 1; i < argc; i++) {
        std::string arg = argv[i];

        if ((arg == "-o" || arg == "-l" || arg == "-g" || arg == "-d") && i + 1 < argc) {
            std::string value = argv[++i];

            if (arg == "-o") {
                outputPath = value;
            } else if (arg == "-l") {
                level = atoi(value.c_str());
            } else if (arg == "-d") {
                database = value;
            } else {
                globalStringsPath = value;
            }
        } else if (arg == "-n") {
            dryRun = true;
        } else if (arg[0] == '-') {
            fputs(kUsage, stderr);
            return 2;
        } else {
            configPath = arg;
        }
    }

    std::string config;

    if (!ReadWholeFile(configPath, config)) {
        fprintf(stderr, "cannot read %s\n", configPath.c_str());
        return 1;
    }

    config = StripXmlComments(config);
    std::string dbcDirectory = XmlValue(config, "DbcDirectory");

    if (dbcDirectory.empty()) {
        fprintf(stderr, "%s has no DbcDirectory\n", configPath.c_str());
        return 1;
    }

    DbConfig db;
    std::string mysqlBlock = XmlValue(config, "MySQL");
    db.host = XmlValue(mysqlBlock, "Host");
    db.user = XmlValue(mysqlBlock, "Username");
    db.password = XmlValue(mysqlBlock, "Password");
    db.database = database;

    if (!XmlValue(mysqlBlock, "Port").empty()) {
        db.port = static_cast<uint32_t>(atoi(XmlValue(mysqlBlock, "Port").c_str()));
    }

    if (outputPath.empty() && (db.host.empty() || db.user.empty())) {
        fprintf(stderr, "%s has no MySQL Host and Username (or pass -o)\n", configPath.c_str());
        return 1;
    }

    if (globalStringsPath.empty()) {
        globalStringsPath = ParentDirectory(dbcDirectory) + "/Interface/FrameXML/GlobalStrings.lua";
    }

    // Connect before the render so a bad login fails fast.
    MYSQL* mysql = nullptr;
    std::map<int32_t, std::string> existing;

    if (outputPath.empty()) {
        mysql = Connect(db);

        if (!mysql || !LoadExisting(mysql, existing)) {
            return 1;
        }
    }

    StormRtlInitialize();
    OsTimeStartup(OsTimeManager::BestAvailable);
    g_clientDbPerformCompress = 0;

    bool loaded = LoadTable<SpellRec>(g_spellDB, dbcDirectory)
        && LoadTable<SpellDurationRec>(g_spellDurationDB, dbcDirectory)
        && LoadTable<SpellRadiusRec>(g_spellRadiusDB, dbcDirectory)
        && LoadTable<SpellRangeRec>(g_spellRangeDB, dbcDirectory)
        && LoadTable<SpellDescriptionVariablesRec>(g_spellDescriptionVariablesDB, dbcDirectory);

    if (!loaded) {
        return 1;
    }

    // Only $z reads it.
    LoadTable<AreaTableRec>(g_areaTableDB, dbcDirectory);

    std::string globalStrings;

    if (!ReadWholeFile(globalStringsPath, globalStrings)) {
        fprintf(stderr, "cannot read %s (pass -g)\n", globalStringsPath.c_str());
        return 1;
    }

    FrameScript_Initialize(0);
    FrameScript_Execute(globalStrings.c_str(), "GlobalStrings.lua", nullptr);

    if (!*FrameScript_GetText("SPELL_DURATION_UNTIL_CANCELLED", -1, GENDER_NOT_APPLICABLE)) {
        fprintf(stderr, "%s did not define the spell strings\n", globalStringsPath.c_str());
        return 1;
    }

    HoT::Spells::SetHeadlessLevel(level);

    static char text[16000];
    std::vector<std::pair<int32_t, std::string>> rows;
    int32_t unresolved = 0;
    SpellRec spell;

    for (int32_t id = g_spellDB.m_minID; id <= g_spellDB.m_maxID; id++) {
        if (!g_spellDB.GetRecord(id, &spell) || !spell.m_description || !*spell.m_description) {
            continue;
        }

        text[0] = 0;
        SpellParserParseText(&spell, text, sizeof(text), 0, 0, 0, 0, 1, 1);
        rows.emplace_back(id, ResolveMarkup(text));

        // Usually a reference to a spell missing from the DBC, the game shows the same.
        if (rows.back().second.find('$') != std::string::npos) {
            unresolved++;
        }
    }

    printf("%zu descriptions rendered at level %d, %d still holding a $ token\n", rows.size(), level, unresolved);

    if (!mysql) {
        if (!WriteSqlFile(outputPath, rows)) {
            return 1;
        }

        printf("written to %s\n", outputPath.c_str());
        return 0;
    }

    std::vector<std::pair<int32_t, std::string>> changed;
    size_t added = 0;

    for (const auto& row : rows) {
        auto it = existing.find(row.first);

        if (it == existing.end()) {
            added++;
            changed.push_back(row);
        } else if (it->second != row.second) {
            changed.push_back(row);
        }
    }

    printf("%s.item_spell_gem_desc: %zu rows, %zu changed, %zu new\n", db.database.c_str(), existing.size(), changed.size() - added, added);

    if (dryRun || changed.empty()) {
        mysql_close(mysql);
        return 0;
    }

    bool applied = ApplyRows(mysql, changed);
    mysql_close(mysql);

    if (!applied) {
        fprintf(stderr, "rolled back, nothing was written\n");
        return 1;
    }

    printf("applied %zu rows\n", changed.size());
    return 0;
}
