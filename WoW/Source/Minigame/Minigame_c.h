#ifndef WOW_SOURCE_MINIGAME_MINIGAME_C_H
#define WOW_SOURCE_MINIGAME_MINIGAME_C_H

#include <cstddef>
#include <cstdint>

class CDataStore;
struct lua_State;

enum MINIGAME_TYPE {
    MINIGAME_NONE = 0,
    MINIGAME_TICTACTOE = 1,
};

enum MINIGAME_STATE : uint32_t {
    MINIGAME_STATE_NONE = 0,
    MINIGAME_STATE_UNKNOWN_2 = 2,
};

struct MiniGameData {
    /* 0000 */ uint32_t m_type;
    /* 0004 */ uint8_t m_state;
    /* 0008 */ uint64_t m_guid;
    /* 0010 */ uint64_t unk_0010;
    /* 0018 */ uint64_t unk_0018;
    /* 0020 */ uint8_t unk_0020;
    /* 0021 */ uint8_t unk_0021;
    /* 0022 */ uint8_t m_board[64];
};

class CMinigameTicTacToe {
    public:
    static bool ValidateMove(MiniGameData* data, uint8_t moveType, int32_t param);
};

class CMinigame_C : public MiniGameData {
    public:
    static CMinigame_C* CreateMinigame(MINIGAME_TYPE type, uint64_t guid);

    CMinigame_C(MINIGAME_TYPE type, uint64_t guid);
    void UpdateSetup(CDataStore* msg);
    void MakeMove(uint8_t moveType, int32_t param);
    virtual void UpdateState(CDataStore* msg) = 0;
    virtual bool ValidateMoveClient(uint8_t moveType, int32_t param) = 0;
    virtual int32_t PushStateToLua(lua_State* L) = 0;
};

class CMinigameTicTacToe_C : public CMinigame_C {
    public:
    CMinigameTicTacToe_C(uint64_t guid);
    virtual void UpdateState(CDataStore* msg);
    virtual bool ValidateMoveClient(uint8_t moveType, int32_t param);
    virtual int32_t PushStateToLua(lua_State* L);
};

#endif
