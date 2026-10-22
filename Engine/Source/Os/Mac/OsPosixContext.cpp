#include "Engine/Source/Os/Linux/OsPosix.h"

// The thread state layout follows the processor, both kinds of Mac are handled here.

static unsigned long long LogValue(uint64_t value) {
    return static_cast<unsigned long long>(value);
}

#if defined(__aarch64__)

void OsPosixContextFrame(const ucontext_t* context, uintptr_t* pc, uintptr_t* fp, uintptr_t* sp) {
    *pc = static_cast<uintptr_t>(context->uc_mcontext->__ss.__pc);
    *fp = static_cast<uintptr_t>(context->uc_mcontext->__ss.__fp);
    *sp = static_cast<uintptr_t>(context->uc_mcontext->__ss.__sp);
}

uint32_t OsPosixContextCodeSegment(const ucontext_t* context) {
    return 0;
}

// ESR_EL1 bit 6 (WnR) is set when a data abort came from a write.
bool OsPosixContextWriteFault(const ucontext_t* context) {
    return (context->uc_mcontext->__es.__esr & (1u << 6)) != 0;
}

const char* OsPosixContextRegistersTitle() {
    return "arm64 Registers";
}

void OsPosixLogContextRegisters(void (*logLineProc)(void*, const char*, ...), void* logLineProcParam, const ucontext_t* context) {
    const auto& ss = context->uc_mcontext->__ss;

    for (int32_t i = 0; i < 28; i += 4) {
        logLineProc(logLineProcParam, "X%-2d=%016llX  X%-2d=%016llX  X%-2d=%016llX  X%-2d=%016llX", i, LogValue(ss.__x[i]), i + 1, LogValue(ss.__x[i + 1]), i + 2, LogValue(ss.__x[i + 2]), i + 3, LogValue(ss.__x[i + 3]));
    }

    logLineProc(logLineProcParam, "X28=%016llX  FP =%016llX  LR =%016llX  SP =%016llX", LogValue(ss.__x[28]), LogValue(ss.__fp), LogValue(ss.__lr), LogValue(ss.__sp));
    logLineProc(logLineProcParam, "PC =%016llX  CPSR=%08X  ESR=%08X  FAR=%016llX", LogValue(ss.__pc), ss.__cpsr, context->uc_mcontext->__es.__esr, LogValue(context->uc_mcontext->__es.__far));
}

#else

void OsPosixContextFrame(const ucontext_t* context, uintptr_t* pc, uintptr_t* fp, uintptr_t* sp) {
    *pc = static_cast<uintptr_t>(context->uc_mcontext->__ss.__rip);
    *fp = static_cast<uintptr_t>(context->uc_mcontext->__ss.__rbp);
    *sp = static_cast<uintptr_t>(context->uc_mcontext->__ss.__rsp);
}

uint32_t OsPosixContextCodeSegment(const ucontext_t* context) {
    return static_cast<uint32_t>(context->uc_mcontext->__ss.__cs & 0xFFFF);
}

// Page-fault error code bit 1 is set for writes.
bool OsPosixContextWriteFault(const ucontext_t* context) {
    return (context->uc_mcontext->__es.__err & 2) != 0;
}

const char* OsPosixContextRegistersTitle() {
    return "x64 Registers";
}

void OsPosixLogContextRegisters(void (*logLineProc)(void*, const char*, ...), void* logLineProcParam, const ucontext_t* context) {
    const auto& ss = context->uc_mcontext->__ss;
    const auto& es = context->uc_mcontext->__es;

    logLineProc(logLineProcParam, "RAX=%016llX  RBX=%016llX  RCX=%016llX  RDX=%016llX", LogValue(ss.__rax), LogValue(ss.__rbx), LogValue(ss.__rcx), LogValue(ss.__rdx));
    logLineProc(logLineProcParam, "RSI=%016llX  RDI=%016llX  RBP=%016llX  RSP=%016llX", LogValue(ss.__rsi), LogValue(ss.__rdi), LogValue(ss.__rbp), LogValue(ss.__rsp));
    logLineProc(logLineProcParam, "R8 =%016llX  R9 =%016llX  R10=%016llX  R11=%016llX", LogValue(ss.__r8), LogValue(ss.__r9), LogValue(ss.__r10), LogValue(ss.__r11));
    logLineProc(logLineProcParam, "R12=%016llX  R13=%016llX  R14=%016llX  R15=%016llX", LogValue(ss.__r12), LogValue(ss.__r13), LogValue(ss.__r14), LogValue(ss.__r15));
    logLineProc(logLineProcParam, "RIP=%016llX  FLG=%08X", LogValue(ss.__rip), static_cast<uint32_t>(ss.__rflags));
    logLineProc(logLineProcParam, "CS =%04X      GS =%04X      FS =%04X      ERR=%08X  TRP=%08X", static_cast<uint32_t>(ss.__cs & 0xFFFF), static_cast<uint32_t>(ss.__gs & 0xFFFF), static_cast<uint32_t>(ss.__fs & 0xFFFF), es.__err, static_cast<uint32_t>(es.__trapno));
}

#endif
