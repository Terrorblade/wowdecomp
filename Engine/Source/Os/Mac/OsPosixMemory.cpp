#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <sys/mman.h>
#include "Engine/Source/Os/Linux/OsPosix.h"

// Without MAP_FIXED the address is a hint, which macOS honors when the range is free.
void* OsPosixMapAt(uintptr_t address, size_t bytes) {
    void* ptr = mmap(reinterpret_cast<void*>(address), bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);

    if (ptr == MAP_FAILED) {
        return nullptr;
    }

    if (reinterpret_cast<uintptr_t>(ptr) != address) {
        munmap(ptr, bytes);
        return nullptr;
    }

    return ptr;
}

void OsPosixEnumerateMappings(bool (*callback)(void* param, uintptr_t start, uintptr_t end), void* param) {
    mach_vm_address_t address = 0;

    for (;;) {
        mach_vm_size_t size = 0;
        vm_region_basic_info_data_64_t info;
        mach_msg_type_number_t count = VM_REGION_BASIC_INFO_COUNT_64;
        mach_port_t object = MACH_PORT_NULL;

        if (mach_vm_region(mach_task_self(), &address, &size, VM_REGION_BASIC_INFO_64, reinterpret_cast<vm_region_info_t>(&info), &count, &object) != KERN_SUCCESS) {
            break;
        }

        if (object != MACH_PORT_NULL) {
            mach_port_deallocate(mach_task_self(), object);
        }

        if (!callback(param, static_cast<uintptr_t>(address), static_cast<uintptr_t>(address + size))) {
            break;
        }

        address += size;
    }
}

bool OsPosixReadMemory(const void* address, void* out, size_t size) {
    mach_vm_size_t read = 0;
    kern_return_t result = mach_vm_read_overwrite(mach_task_self(), reinterpret_cast<mach_vm_address_t>(address), size, reinterpret_cast<mach_vm_address_t>(out), &read);
    return result == KERN_SUCCESS && read == size;
}
