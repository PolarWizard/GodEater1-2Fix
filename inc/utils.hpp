/*
 * MIT License
 *
 * Copyright (c) 2025 Dominik Protasewicz
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#pragma once

#include <windows.h>
#include <vector>
#include <string>
#include <cstdint>
#include <span>

// 3rd party includes
#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "yaml-cpp/yaml.h"
#include "safetyhook.hpp"

#define LOG(STRING, ...) spdlog::info("{} : " STRING, __func__, ##__VA_ARGS__)

namespace
{
    typedef uint8_t  u8;
    typedef uint16_t u16;
    typedef uint32_t u32;
    typedef uint64_t u64;
    typedef int8_t   i8;
    typedef int16_t  i16;
    typedef int32_t  i32;
    typedef int64_t  i64;
    typedef float    f32;
    typedef double   f64;
}

namespace Utils
{
    class ModuleInfo {
    public:
        HMODULE address;
        std::string name;
        ModuleInfo(HMODULE address) : address(address) {}
    };

    class SignatureHook {
    public:
        std::string signature;
        u64 offset;
        SignatureHook(std::string signature, u64 offset = 0) : signature(signature), offset(offset) {}
    };

    /**
     * @brief Retrieves information about the compiler being used.
     * @details This function returns a string containing the name and version of the
     *      compiler. It checks for several well-known compilers and formats the version
     *      information accordingly:
     *
     * - **GCC:** The version is formatted as "GCC - major.minor.patch".
     * - **Clang:** The version is formatted as "Clang - major.minor.patch".
     * - **MSVC:** The version is formatted as "MSVC - version number".
     * - **Unknown Compiler:** If the compiler is not recognized, it returns "UNKNOWN".
     *
     * @return `std::string` containing the compiler name and version.
     */
    std::string getCompilerInfo();

    /**
     * @brief Converts memory bytes into string representation.
     * @details Converts the bytes referenced by the `bytes` parameter into a IDA-style byte string.
     *      The output is formatted as space-separated byte values, appearing in the same order
     *      as they exist in memory. For example, if an integer `0x12345678` is passed the
     *      function returns `"78 56 34 12"` on a little-endian system.
     *
     * @param bytes A span of bytes representing the memory region.
     * @return std::string containing the IDA-style byte string.
     *
     * @code
     * float a = 3.5555556; // In hex: 0x40638E39
     * std::string myString = bytesToString(std::as_bytes(std::span{&value, 1}));
     * std::cout << myString << std::endl; // Prints "39 8E 63 40"
     *
     * std::vector<u8> data = {0x40, 0x63, 0x8E, 0x39};
     * std::string myString = bytesToString(data);
     * std::cout << myString << std::endl; // Prints "40 63 8E 39"
     * @endcode
     */
    std::string bytesToString(std::span<const u8> bytes);

    /**
     * @brief Get the width and height of the desktop in pixels.
     *
     * @return std::pair<u32, u32> containing the width and height.
     */
    std::pair<u32, u32> getDesktopDimensions();

    /**
     * @brief Patch an area of memory with a pattern.
     * @details Overwrites memory at `address` using the provided pattern. The `pattern`
     *      must be formatted as a space-separated hex string (e.g., `"DE AD BE EF"`).
     *      The function modifies memory at the specified address, spanning the number
     *      of bytes determined by the pattern length. Proper care should be taken to
     *      avoid segmentation faults or corruption of unintended memory regions.
     *
     * @param address Memory address to patch.
     * @param pattern IDA-style byte array pattern.
     */
    void patch(u64 address, std::string& pattern);

    /**
     * @brief Scan for a given byte pattern in a module.
     * @details Searches the specified module's memory for occurrences of the given
     *      IDA-style byte pattern. Each match is appended to the `address` vector.
     *      Wildcard bytes ("??") can be used to match any byte in the pattern.
     *
     * @param module Base address of the module to scan.
     * @param signature IDA-style byte array pattern.
     * @param address Vector to store found addresses.
     *
     * @return uintptr_t containing the address of the first hit if the signature is
     *      found else 0.
     */
    uintptr_t patternScan(void* module, std::string& signature);

    /**
     * @brief Injects a mid-function hook based on a signature pattern match.
     *
     * @tparam Func The type of the callback function.
     * @param enable If true, the hook will be injected; otherwise, it is skipped.
     * @param module The module to scan for the signature.
     * @param hook The signature pattern and offset information for the hook.
     * @param callback The function to execute when the hook is triggered.
     *
     * @details
     * This function scans the specified module for the given signature pattern.
     * If a match is found, it calculates the absolute and relative addresses,
     * logs the location, and applies a mid-function hook at the computed address.
     *
     * @note This function only hooks the first match found in the module.
     *
     * @see Utils::patternScan
     */
    template <typename Func>
    void injectHook(bool enable, Utils::ModuleInfo& module, Utils::SignatureHook& hook, Func&& callback) {
        LOG("Fix {}", enable ? "Enabled" : "Disabled");
        if (enable) {
            uintptr_t addr = Utils::patternScan(module.address, hook.signature);
            if (addr != 0) {
                u64 hit = addr;
                u64 absAddr = hit;
                u64 relAddr = hit - reinterpret_cast<u64>(module.address);
                LOG("Found '{}' @ {:s}+{:x}", hook.signature, module.name, relAddr);
                u64 hookAbsAddr = absAddr + hook.offset;
                u64 hookRelAddr = relAddr + hook.offset;
                static SafetyHookMid aspectRatioHook = safetyhook::create_mid(
                    reinterpret_cast<void*>(hookAbsAddr),
                    callback
                );
                LOG("Hooked @ {:s}+{:x}", module.name, hookRelAddr);
            }
            else {
                LOG("Did not find '{}'", hook.signature);
            }
        }
    }
}
