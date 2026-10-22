#ifndef HOT_MSDF_CONSOLE_H
#define HOT_MSDF_CONSOLE_H

#include <cstdint>
#include <string>

// The text console the font pre-generation prompts run in, and its one-instance lock.
namespace HoT::MSDF::Console {

class Session {
    public:
    Session();
    ~Session();

    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;

    // False when no console could be had, pre-generation is skipped then.
    bool IsOpen() const { return this->m_open; }

    private:
    void* m_state = nullptr;
    bool m_open = false;
};

bool ReadInt(int& value);
bool ReadHex(uint32_t& value);
bool ReadDouble(double& value);

// Held while pre-generation runs, false when another client already holds it.
bool AcquireLock(const std::string& path);
void ReleaseLock();

}

#endif
