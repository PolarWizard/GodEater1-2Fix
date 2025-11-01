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

// System includes
#include <windows.h>
#include <psapi.h>
#include <shlwapi.h>
#include <fstream>
#include <iostream>
#include <string>
#include <filesystem>
#include <format>
#include <numeric>
#include <numbers>
#include <cmath>
#include <cstdint>
#include <algorithm>
#include <bit>

// Local includes
#include "utils.hpp"

// Macros
#define VERSION "1.0.0"

// .yml to struct
typedef struct resolution_t {
    u32 width;
    u32 height;
    f32 aspectRatio;
} resolution_t;

typedef struct constrainHud_t {
    bool enable;
} constrainHud_t;

typedef struct features_t {
    constrainHud_t constrainHud;
} features_t;

typedef struct yml_t {
    std::string name;
    bool masterEnable;
    resolution_t resolution;
    features_t feature;
} yml_t;

// Globals
Utils::ModuleInfo module(GetModuleHandle(nullptr));

u32 nativeWidth = 0;
u32 nativeOffset = 0;
f32 nativeAspectRatio = (16.0f / 9.0f);
f32 widthScalingFactor = 0;

SafetyHookInline readFileHook{};
bool isMoviePlaying = false;

YAML::Node config = YAML::LoadFile("GodEater1-2Fix.yml");
yml_t yml;

/**
 * @brief Initializes logging for the application.
 *
 * @return void
 */
void logInit() {
    // spdlog initialisation
    auto logger = spdlog::basic_logger_mt("GodEater1-2Fix", "GodEater1-2Fix.log", true);
    spdlog::set_default_logger(logger);
    spdlog::flush_on(spdlog::level::debug);

    // Get game name and exe path
    WCHAR exePath[_MAX_PATH] = { 0 };
    GetModuleFileNameW(module.address, exePath, MAX_PATH);
    std::filesystem::path exeFilePath = exePath;
    module.name = exeFilePath.filename().string();

    // Log module details
    LOG("-------------------------------------");
    LOG("Compiler: {:s}", Utils::getCompilerInfo());
    LOG("Compiled: {:s} at {:s}", __DATE__, __TIME__);
    LOG("Version: {:s}", VERSION);
    LOG("Module Name: {:s}", module.name);
    LOG("Module Path: {:s}", exeFilePath.string());
    LOG("Module Addr: 0x{:x}", reinterpret_cast<u64>(module.address));
}

/**
 * @brief Reads and parses configuration settings from a YAML file.
 *
 * @return void
 */
void readYml() {
    yml.name = config["name"].as<std::string>();

    yml.masterEnable = config["masterEnable"].as<bool>();

    yml.resolution.width = config["resolution"]["width"].as<u32>();
    yml.resolution.height = config["resolution"]["height"].as<u32>();

    yml.feature.constrainHud.enable = config["features"]["constrainHud"]["enable"].as<bool>();

    if (yml.resolution.width == 0 || yml.resolution.height == 0) {
        std::pair<int, int> dimensions = Utils::getDesktopDimensions();
        yml.resolution.width  = dimensions.first;
        yml.resolution.height = dimensions.second;
    }
    yml.resolution.aspectRatio = static_cast<f32>(yml.resolution.width) / static_cast<f32>(yml.resolution.height);
    nativeWidth = (16.0f / 9.0f) * static_cast<f32>(yml.resolution.height);
    nativeOffset = static_cast<f32>(yml.resolution.width - nativeWidth) / 2.0f;
    widthScalingFactor = static_cast<f32>(yml.resolution.width) / static_cast<f32>(nativeWidth);

    // Get that info!
    LOG("Name: {}", yml.name);
    LOG("MasterEnable: {}", yml.masterEnable);
    LOG("Resolution.Width: {}", yml.resolution.width);
    LOG("Resolution.Height: {}", yml.resolution.height);
    LOG("Resolution.AspectRatio: {}", yml.resolution.aspectRatio);
    LOG("Normalized Width: {}", nativeWidth);
    LOG("Normalized Offset: {}", nativeOffset);
    LOG("Width Scaling Factor: {}", widthScalingFactor);
}

/**
 * @brief Fixes aspect ratio to desired resolution.
 *
 * @details
 * The game does not support aspect ratios other than 16:9. This fix hooks the game's function
 * where aspect ratio is written and overrides it with the desired aspect ratio.
 *
 * How was this found?
 * In the exe itself the game stores the native resolution of 1920x1080, in hex '80 07 00 00 38 04 00 00'
 * at address 0x11BB0D8. Of course editing the exe can be done here to change the native resolution, but
 * if we can hook or patch process memory then that will always be the prefered solution. And this thankfully
 * is a case where we can hook the game's memory.
 *
 * The function that calculates the aspect ratio is as follows:
 * 1  - ger.exe+1352EAF  - A1 D8C65B01                    - mov eax,dword ptr ds:[15BC6D8]
 * 2  - ger.exe+1352EB4  - 66:0F6EC0                      - movd xmm0,eax
 * 3  - ger.exe+1352EB8  - F3:0FE6C0                      - cvtdq2pd xmm0,xmm0
 * 4  - ger.exe+1352EBC  - C1E8 1F                        - shr eax,1F
 * 5  - ger.exe+1352EBF  - F2:0F5804C5 50864001           - addsd xmm0,qword ptr ds:[eax*8+1408650]
 * 6  - ger.exe+1352EC8  - A1 DCC65B01                    - mov eax,dword ptr ds:[15BC6DC]
 * 7  - ger.exe+1352ECD  - 66:0F5AC8                      - cvtpd2ps xmm1,xmm0
 * 8  - ger.exe+1352ED1  - 66:0F6EC0                      - movd xmm0,eax
 * 9  - ger.exe+1352ED5  - F3:0FE6C0                      - cvtdq2pd xmm0,xmm0
 * 10 - ger.exe+1352ED9  - C1E8 1F                        - shr eax,1F
 * 11 - ger.exe+1352EDC  - F2:0F5804C5 50864001           - addsd xmm0,qword ptr ds:[eax*8+1408650]
 * 12 - ger.exe+1352EE5  - 66:0F5AC0                      - cvtpd2ps xmm0,xmm0
 * 13 - ger.exe+1352EE9  - F3:0F5EC8                      - divss xmm1,xmm0
 * 14 - ger.exe+1352EED  - F3:0F110D 34F26F01             - movss dword ptr ds:[16FF234],xmm1
 * 15 - ger.exe+1352EF5  - C3                             - ret
 *
 * The lines of value include:
 *     1. Native width of 1920 is read
 *     6. Native height of 1080 is read
 *     13. Aspect ratio is calculated 1920/1080 and stored in xmm1
 *     14. Aspect ratio is written to memory
 *
 * Hooking here would be ideal, but this happens way to early in the game code execution, and this dll
 * has not been injected yet, and we need to look further where this value is used.
 *
 * Analyzing this memory location we see the following instructions make accesses when game is rendering
 * 3D objects, it wont get any action on the main menu or non 3D scenes:
 * 1 - ger.exe+15C403F  - 0F1105 34F26F01                - movups xmmword ptr ds:[16FF234],xmm0
 * 2 - ger.exe+15C3442  - F3:0F1005 34F26F01             - movss xmm0,dword ptr ds:[16FF234]
 * 3 - ger.exe+15C34FE  - F3:0F1005 34F26F01             - movss xmm0,dword ptr ds:[16FF234]
 *
 * Naturally out of these hits the only one we care about is the write that happens again. Even though
 * the game calculates the aspect ratio at the start of the game loop, it continuosly plows the native
 * aspect ratio over and over again which comes from ebx+C:
 * 1 - ger.exe+15C403A  - F3:0F1043 0C                   - movss xmm0,[ebx+C]
 *
 * A hook is placed on line 1, where the xmm0 is written to memory, where we inject the desired aspect
 * ratio value. And now in game the game is correctly rendering the area that it should be.
 *
 * @return void
 */
void aspectRatioFix() {
    Utils::SignatureHook hook("F3 0F 11 05 ?? ?? ?? ??    E8 ?? ?? ?? ??    89 EC");

    bool enable = yml.masterEnable;
    Utils::injectHook(enable, module, hook,
        [](SafetyHookContext& ctx) {
            ctx.xmm0.f32[0] = yml.resolution.aspectRatio;
        }
    );
}

/**
 * @brief Fixes the resolution to make ultrawide possible.
 *
 * @details
 * Game does not support ultrawide natively, any resolutions outside of 16:9 will spawn black bars
 * on the side. This gets rid of black bars and expands the game window to fill the screen. Note this
 * is not a viewport fix, the game will still render at 16:9, it just expands the render window to fill
 * the screen.
 *
 * How was this found?
 * I did this a long time ago and don't remember the thought process that went into finding this. That's
 * lost knowledge at this point even with trying to retrace from this point backwards. On the brightside
 * it works though!!!
 *
 * @return void
 */
void resolutionFix() {
    Utils::SignatureHook hook(
        "76 ??    F3 0F 59 05 ?? ?? ?? ??    F3 0F 5E 05 ?? ?? ?? ??    E8 ?? ?? ?? ??",
        18
    );

    bool enable = yml.masterEnable;
    Utils::injectHook(enable, module, hook,
        [](SafetyHookContext& ctx) {
            if (isMoviePlaying == false) {
                ctx.xmm0.f32[0] = static_cast<float>(yml.resolution.width);
            }
        }
    );
}

/**
 * @brief Fixes HUD elements by constraining them to 16:9.
 *
 * @details
 * By default due to other fixes that make ultrawide possible also effect the HUD and therefore will
 * be stretched to fill the screen. This fix constrains the HUD back to 16:9.
 *
 * How was this found?
 * Anything dealing with HUD and UI is always tricky to find and you are at the mercy of the game
 * developer, engine, and whatever else as all that determines how the HUD and UI elements are done,
 * how they are anchored and what effects they have.
 *
 * The typical trick is to scan the game for 1.0f values and modify them slightly and see what effect
 * that has in game if any. Hopefully you get a hit eventually that actually effects the HUD, and for
 * this game that eventually happened.
 *
 * With a single 1.0f that controlled the HUD we found some code around GER.exe+1d510d4 which makes use
 * of this value and from that point we examined what the code was doing and with that we found some more
 * clues and other memory locations that are of interest.
 *
 * We found two values of interest at ctx.eax+0x00 and ctx.eax+0x30. They were played around with and
 * once the peices clicked and was understood what effect they had it was pretty simple to get some
 * hook code up and running which would do what we wanted.
 *
 * Now the next issue that was only part of the UI, there were still parts of the UI that were not
 * effected by the fix just implemented, so back to scanning and modifying 1.0f values. Eventually
 * got a promising lead on a value in dynamic memory that fixed the rest of the UI and was accessed
 * in the same location as the first 1.0f hit earlier. Given the first was in the game data section
 * that could easily be hooked up to get the fix, but dynamic memory is tricky we cant just hook up
 * some address and call it a day as the address will change from boot to boot. We need to figure out
 * a way to isolate this from other addresses held in ctx.eax which we do not wanna touch so with
 * enough experimentation and trial and error we could make a basic check to see if ctx.eax+0x30 is
 * 0xBF80_0000 and if ctx.eax+0x3C is 0x3F80_0000 then if both checkout we are dealing with UI data
 * to which we can make memory modifications and just like that UI is fixed and nicely constrained to
 * 16:9 just as intended.
 *
 * EDIT: v1.0.1
 * The above is wrong for the calculation. As of issue #1 someone reported that the HUD does not center
 * correctly at 21:9. After investigating this it turns out that there is a pretty complex formula you
 * need for ctx.eax+0x00, for 32:9 it was simple to get the center since 32:9 is just 2x of 16:9, but
 * for 21:9 things where weird. Ultimately I figured out the relationship between ctx.eax+0x00 and
 * ctx.eax+0x30, where a part of the calculation for ctx.eax+0x30 is needed in ctx.eax+0x00 and the
 * final formula/calculation can be seen in the code below. The reason why this worked flawlessly in
 * 32:9 was because the calculation will always 1 / width:
 * Reverse engineered equation for ctx.eax+0x00:
 *       1                    1                 2       nativeWidth
 * ------------ * ------------------------ = ------- * -------------
 *  (width / 2)     (width / nativeWidth)     width        width
 *
 * if 3440x1440 (21:9) then:
 * (2 / 3440) * (2560 / 3440) = 0.00058139535, unnormalized = 1/0.00058139535 = ~2311
 *
 * if 7680x2160 (32:9) then:
 * (2 / 7680) * (3840 / 7680) = 1 / 7680 =  0.00013020833, unnormalized = 1/0.00013020833 = 7680
 *                                 ^
 *                                 This is what I had initially
 *                                 in the mod and you can see
 *                                 where that comes from
 *
 * @return void
 */
void hudElementsFix() {
    Utils::SignatureHook hook("F3 0F 6F 00    F3 0F 7F 41 0C    F3 0F 6F 40 10");

    bool enable = yml.masterEnable && yml.feature.constrainHud.enable;
    Utils::injectHook(enable, module, hook,
        [](SafetyHookContext& ctx) {
            u32 scaler0 = *reinterpret_cast<u32*>(ctx.eax + 0x30);
            u32 scaler1 = *reinterpret_cast<u32*>(ctx.eax + 0x3C);
            if (((scaler0 & 0xBF000000) == 0xBF000000) && ((scaler1 & 0x3F000000) == 0x3F000000)) {
                f32 ratio = static_cast<f32>(nativeWidth) / static_cast<f32>(yml.resolution.width);
                *reinterpret_cast<f32*>(ctx.eax + 0x00) = (2.0f / static_cast<f32>(yml.resolution.width)) * ratio;
                *reinterpret_cast<f32*>(ctx.eax + 0x30) = ratio * -1.0f;
            }
        }
    );
}

/**
 * @brief Hook to intercept ReadFile calls.
 *
 * @details
 * The hook is placed on the ReadFile function of the kernel32.dll.
 * The hook intercepts the ReadFile call and checks the file path and if the file has a .wmv extension
 * then the isMoviePlaying variable is set to true.
 * None of the input parameters are dirtied, only hFile is read to determine the file extension.
 *
 * @param hFile A handle to the device.
 * @param lpBuffer A pointer to the buffer that receives the data read from a file or device.
 * @param nNumberOfBytesToRead The maximum number of bytes to be read.
 * @param lpNumberOfBytesRead A pointer to the variable that receives the number of bytes read when
 *  using a synchronous hFile parameter.
 * @param lpOverlapped A pointer to an OVERLAPPED structure is required if the hFile parameter was
 *  opened with FILE_FLAG_OVERLAPPED, otherwise it can be NULL.
 * @return BOOL If the function succeeds, the return value is nonzero (TRUE).
 *
 * @note https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-readfile
 */
BOOL WINAPI kernelBaseDllReadFileHook(
    HANDLE hFile,
    LPVOID lpBuffer,
    DWORD nNumberOfBytesToRead,
    LPDWORD lpNumberOfBytesRead,
    LPOVERLAPPED lpOverlapped
) {
    char fileName[MAX_PATH] = {0};
    if (GetFinalPathNameByHandleA(hFile, fileName, MAX_PATH, FILE_NAME_NORMALIZED)) {
        std::string pathStr(fileName);
        if (pathStr.starts_with("\\\\?\\")) {
            pathStr = pathStr.substr(4);
        }
        std::filesystem::path filePath(pathStr);
        std::string extension = filePath.extension().string();
        isMoviePlaying = extension == ".wmv" ? true : false;
    }
    return readFileHook.stdcall<BOOL>(hFile, lpBuffer, nNumberOfBytesToRead, lpNumberOfBytesRead, lpOverlapped);
};

/**
 * @brief Fixes movies by constraining them to 16:9.
 *
 * @details
 * Given the resolution fix earlier to get the game to run at ultrawide resolutions natively, this
 * introduces a problem. Movies are prerendered at 16:9, but the game is rendered at ultrawide, which
 * stretches the movies to fill the screen. This fix constrains movies back to 16:9, by disabling the
 * resolution fix when movies are being played.
 *
 * How was this found?
 * This is where things get a bit freaky. The game underhood seems to support a scripting engine and
 * most of the game is scripted through some properietary scripting language. Although with my research
 * there does not seem to be much work done reverse engineering it, but there is hints of it in those
 * .qpck files if you open them via a hex editor. Anyway, this makes it very hard to track down where
 * exactly movies begin to play and when they are considered to be finished.
 *
 * In the game root folder you will see .qpck files, where the game stores practically everything:
 * event triggers, game logic, cutscene handling, AI behavior, sounds to play, etc.
 * If you navigate from the root folder to data/GameData you get a lot of folders, which host sound
 * files mainly, but we are interested only in the movie folder. All the movie files are .wmv.
 *
 * Scanning for movie strings within the exe itself gets no hits, but the .qpck files do get hits,
 * another hint that the game does not have code which directly handles movies, but gets off loaded
 * to the suspected scripting language.
 *
 * Anyway, this is where things get hard how do we figure out where movies begin to play, well using
 * ProcMon we can see stack traces where the game reads files. With some filtering we can see that the
 * game makes repeatedly reads the movie file in chunks, meaning its streamed. The stack trace proves
 * this as the game exe eventually makes a call to quartz.dll, a Windows system library that's part
 * of DirectShow, a multimedia framework developed by Microsoft for video and audio playback, capture,
 * and streaming.
 *
 * Under the hood quartz.dll eventually calls ReadFile function from KernelBase.dll, so the game itself
 * does not even touch the movie file itself, it only provides the path which makes sense since those
 * .qpck files are littered with paths to the movie and sound files.
 *
 * As I said in the beginning this is where it gets freaky, we cannot rely on hooking based on calls
 * to quartz.dll functions, we need to go even deeper and hook the ReadFile function from KernelBase.dll,
 * because the first param to ReadFile is the file handle, which can be easily used to figure out what
 * file is being read. And that is exactly what is being done in the fix, we figure out from the file
 * handle if it is a file ending with .wmv and if it is a movie is currently playing, if its anything
 * else a movie is not playing. This works because the movie itself is read in chunks and the game
 * is basically IO blocked until the movie is fully played or the user skips it, where it goes back
 * to reading the .qpck files either to get the next script to execute or get some asset or do something
 * else.
 *
 * But this is the most reliable way to detect if and when a movie is playing. No fancy tricks in the
 * game code, just abusing Windows APIs to our benefit.
 *
 * @return void
 */
void moviesFix()
{
    bool enable = yml.masterEnable;
    if (enable == true) {
        std::string targetDll = "KernelBase.dll";
        HMODULE kernelBaseAddr = GetModuleHandleA(targetDll.c_str());
        if (!kernelBaseAddr) {
            LOG("Failed to get handle to {:s}", targetDll.c_str());
            return;
        }

        std::string dllFunction = "ReadFile";
        void* readFileAddr = GetProcAddress(kernelBaseAddr, dllFunction.c_str());
        if (!readFileAddr) {
            LOG("Failed to get address of {:s}", dllFunction.c_str());
            return;
        }

        readFileHook = safetyhook::create_inline(reinterpret_cast<void*>(readFileAddr), reinterpret_cast<void*>(&kernelBaseDllReadFileHook));
        LOG("Hooked {:s} @ {:s}+{:x}", dllFunction.c_str(), targetDll.c_str(), reinterpret_cast<u64>(readFileAddr) - reinterpret_cast<u64>(kernelBaseAddr));
    }
}

/**
 * @brief This function serves as the entry point for the DLL. It performs the following tasks:
 * 1. Initializes the logging system.
 * 2. Reads the configuration from a YAML file.
 * 3. Applies a center UI fix.
 *
 * @param lpParameter Unused parameter.
 * @return Always returns TRUE to indicate successful execution.
 */
DWORD WINAPI Main(void* lpParameter) {
    logInit();
    readYml();
    moviesFix();
    aspectRatioFix();
    resolutionFix();
    hudElementsFix();
    return true;
}

/**
 * @brief Entry point for a DLL, called by the system when the DLL is loaded or unloaded.
 *
 * This function handles various events related to the DLL's lifetime and performs actions
 * based on the reason for the call. Specifically, it creates a new thread when the DLL is
 * attached to a process.
 *
 * @details
 * The `DllMain` function is called by the system when the DLL is loaded or unloaded. It handles
 * different reasons for the call specified by `ul_reason_for_call`. In this implementation:
 *
 * - **DLL_PROCESS_ATTACH**: When the DLL is loaded into the address space of a process, it
 *   creates a new thread to run the `Main` function. The thread priority is set to the highest,
 *   and the thread handle is closed after creation.
 *
 * - **DLL_THREAD_ATTACH**: Called when a new thread is created in the process. No action is taken
 *   in this implementation.
 *
 * - **DLL_THREAD_DETACH**: Called when a thread exits cleanly. No action is taken in this implementation.
 *
 * - **DLL_PROCESS_DETACH**: Called when the DLL is unloaded from the address space of a process.
 *   No action is taken in this implementation.
 *
 * @param hModule Handle to the DLL module. This parameter is used to identify the DLL.
 * @param ul_reason_for_call Indicates the reason for the call (e.g., process attach, thread attach).
 * @param lpReserved Reserved for future use. This parameter is typically NULL.
 * @return BOOL Always returns TRUE to indicate successful execution.
 */
BOOL APIENTRY DllMain(
    HMODULE hModule,
    DWORD  ul_reason_for_call,
    LPVOID lpReserved
) {
    HANDLE mainHandle;
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
        mainHandle = CreateThread(NULL, 0, Main, 0, NULL, 0);
        if (mainHandle)
        {
            SetThreadPriority(mainHandle, THREAD_PRIORITY_HIGHEST);
            CloseHandle(mainHandle);
        }
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}
