#include "Engine/Source/Os/Linux/OsPosix.h"

static unsigned long long LogValue(greg_t value) {
    return static_cast<unsigned long long>(value);
}

void OsPosixContextFrame(const ucontext_t* context, uintptr_t* pc, uintptr_t* fp, uintptr_t* sp) {
    *pc = static_cast<uintptr_t>(context->uc_mcontext.gregs[REG_RIP]);
    *fp = static_cast<uintptr_t>(context->uc_mcontext.gregs[REG_RBP]);
    *sp = static_cast<uintptr_t>(context->uc_mcontext.gregs[REG_RSP]);
}

uint32_t OsPosixContextCodeSegment(const ucontext_t* context) {
    return static_cast<uint32_t>(context->uc_mcontext.gregs[REG_CSGSFS] & 0xFFFF);
}

// Page-fault error code bit 1 is set for writes.
bool OsPosixContextWriteFault(const ucontext_t* context) {
    return (context->uc_mcontext.gregs[REG_ERR] & 2) != 0;
}

const char* OsPosixContextRegistersTitle() {
    return "x64 Registers";
}

void OsPosixLogContextRegisters(void (*logLineProc)(void*, const char*, ...), void* logLineProcParam, const ucontext_t* context) {
    const greg_t* gregs = context->uc_mcontext.gregs;
    uint64_t csgsfs = static_cast<uint64_t>(gregs[REG_CSGSFS]);

    logLineProc(logLineProcParam, "RAX=%016llX  RBX=%016llX  RCX=%016llX  RDX=%016llX", LogValue(gregs[REG_RAX]), LogValue(gregs[REG_RBX]), LogValue(gregs[REG_RCX]), LogValue(gregs[REG_RDX]));
    logLineProc(logLineProcParam, "RSI=%016llX  RDI=%016llX  RBP=%016llX  RSP=%016llX", LogValue(gregs[REG_RSI]), LogValue(gregs[REG_RDI]), LogValue(gregs[REG_RBP]), LogValue(gregs[REG_RSP]));
    logLineProc(logLineProcParam, "R8 =%016llX  R9 =%016llX  R10=%016llX  R11=%016llX", LogValue(gregs[REG_R8]), LogValue(gregs[REG_R9]), LogValue(gregs[REG_R10]), LogValue(gregs[REG_R11]));
    logLineProc(logLineProcParam, "R12=%016llX  R13=%016llX  R14=%016llX  R15=%016llX", LogValue(gregs[REG_R12]), LogValue(gregs[REG_R13]), LogValue(gregs[REG_R14]), LogValue(gregs[REG_R15]));
    logLineProc(logLineProcParam, "RIP=%016llX  FLG=%08X", LogValue(gregs[REG_RIP]), static_cast<uint32_t>(gregs[REG_EFL]));
    logLineProc(logLineProcParam, "CS =%04X      GS =%04X      FS =%04X      ERR=%08X  TRP=%08X", static_cast<uint32_t>(csgsfs & 0xFFFF), static_cast<uint32_t>((csgsfs >> 16) & 0xFFFF), static_cast<uint32_t>((csgsfs >> 32) & 0xFFFF), static_cast<uint32_t>(gregs[REG_ERR]), static_cast<uint32_t>(gregs[REG_TRAPNO]));
}
