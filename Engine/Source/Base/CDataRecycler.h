#ifndef ENGINE_SOURCE_BASE_CDATARECYCLER_H
#define ENGINE_SOURCE_BASE_CDATARECYCLER_H

#include <cstdint>

class CDataRecycler {
    public:

    enum : int32_t {
        eDefaultMaxNodes = 0x7FFFFFFF,
    };

    enum : uint32_t {
        eDefaultNodesPerBlock = 16,
    };

    struct Node {
        /* 0000 */ Node* m_next;
        /* 0004 */ void* m_data;
        /* 0008 */ uint32_t m_bytes;
    };

    struct NodeBlock {
        /* 0000 */ NodeBlock* m_next;
        /* 0004 */ Node m_nodes[1];
    };

    /* 0004 */ int32_t m_nodesRecyclable;
    /* 0008 */ uint32_t m_nodesPerBlock;
    /* 000C */ NodeBlock* m_nodeBlockList;
    /* 0010 */ Node* m_nodeFullList;
    /* 0014 */ Node* m_nodeEmptyList;

    CDataRecycler(uint32_t nodesPerBlock, int32_t maxNodes);
    virtual ~CDataRecycler();
    virtual void Clear();
    virtual void* AllocData(uint32_t allocBytes, uint32_t* bytes, const char* fileName, int32_t lineNumber);
    virtual void* ReallocData(void* data, uint32_t allocBytes, uint32_t* bytes, const char* fileName, int32_t lineNumber);
    virtual void FreeData(void* data, const char* fileName, int32_t lineNumber);
    void GetData(void*& data, uint32_t& bytes, const char* fileName, int32_t lineNumber);
    void PutData(void* data, uint32_t bytes, const char* fileName, int32_t lineNumber);

    private:
    void Link(void** list, void* item);
    void Link(Node** list, NodeBlock* nodeBlock);
    void* Unlink(void** list);
};

#endif
