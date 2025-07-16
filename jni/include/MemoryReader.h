#pragma once
#include "kernel.hpp"
#include <cstdint>
#include <cstring>
#include <string>

class MemoryReader {
public:

    template<typename T>
    static T Read(uint64_t addr) {
        return driver->read<T>(addr);
    }

    static void Read(uint64_t addr, void* buffer, int size) {
        driver->read(addr, buffer, size);
    }

    template<typename T>
    static bool Write(uint64_t addr, T data) {
        return driver->write<T>(addr, data);
    }

    static void Write(uint64_t addr, void* buffer, int size) {
        driver->write(addr, buffer, size);
    }

    static uint64_t GetModuleBase(const std::string& soName) {
        return driver->get_mod_base(soName);
    }

    static int GetPid(const std::string& packageName) {
        return driver->get_pid(packageName);
    }

    static bool IsValidAddress(uint64_t addr) {
        return !driver->is_unvalid(addr);
    }

    static bool IsValid() {
        return !driver->is_unvalid();
    }
}; 