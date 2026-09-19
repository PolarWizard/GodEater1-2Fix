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
#define VERSION "1.0.2"

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
    nativeWidth = static_cast<u32>((16.0f / 9.0f) * static_cast<f32>(yml.resolution.height));
    nativeOffset = static_cast<u32>(static_cast<f32>(yml.resolution.width - nativeWidth) / 2.0f);
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
 * @brief Forces the game to use our resolution instead of its own saved config.ini value.
 *
 * @details
 * The game persists its own graphics settings (resolution included) in a config.ini under
 * `%LOCALAPPDATA%\<publisher>\<game>\config.ini`, read via the `[GFX]` section's
 * `Resolution_Width`/`Resolution_Height` keys - entirely independent of GodEater1-2Fix.yml. If
 * that file's resolution does not match the resolution requested in GodEater1-2Fix.yml, the game
 * hangs very early during boot (process alive, no window ever created) - see docs/RE_LOG.md for
 * the full reverse engineering trail.
 *
 * How was this found?
 * Reverse engineering the game's "Application start" routine (the one-shot boot init that reads
 * config.ini and creates the game's D3D9 device/window) shows it reads Resolution_Width and
 * Resolution_Height from config.ini into two stack locals, then copies them into the running
 * application-context object it's constructing, right before that context is handed off to
 * device creation:
 *   ger.exe+809E4  - 8B85 2CFFFFFF     - mov eax,[ebp-D4]   ; eax = parsed Resolution_Width
 *   ger.exe+809EA  - B9 01000000       - mov ecx,1
 *   ger.exe+809EF  - 83BD 30FFFFFF 01  - cmp dword ptr [ebp-D0],1
 *   ger.exe+809F6  - FFB5 28FFFFFF     - push dword ptr [ebp-D8]
 *   ger.exe+809FC  - 89 46 70          - mov [esi+70],eax   ; app context Width = eax
 *   ger.exe+809FF  - 8B85 34FFFFFF     - mov eax,[ebp-CC]   ; eax = parsed Resolution_Height
 *   ger.exe+80A05  - 89 46 74          - mov [esi+74],eax   ; app context Height = eax
 *
 * esi holds the application-context object for the whole function (set once from ecx at function
 * entry and never reassigned), so this is the single place in the whole boot sequence where the
 * value read from config.ini becomes "real" as far as the rest of the engine (device creation,
 * window sizing, everything downstream) is concerned. Hooking here edits the config.ini-derived
 * value directly in the game's own memory, at the exact moment the game itself picks it up,
 * entirely within the game's own process memory - no external file ever gets touched, and no
 * dependency on any Windows API export existing under a particular name.
 *
 * A hook is placed on the first store (line 5 above, "mov [esi+70],eax"): eax is overwritten with
 * our desired width right before that instruction executes, so the store faithfully writes our
 * value. The stack slot backing the not-yet-executed height load two instructions later
 * ([ebp-CC], read by line 6) is patched directly in the same callback, so by the time lines 6-7
 * run natively they pick up our height too. One hook, both values.
 *
 * @return void
 */
void configResolutionFix() {
    Utils::SignatureHook hook(
        "8B 85 2C FF FF FF    B9 01 00 00 00    83 BD 30 FF FF FF 01    FF B5 28 FF FF FF    89 46 70",
        24
    );

    bool enable = yml.masterEnable;
    Utils::injectHook(enable, module, hook,
        [](SafetyHookContext& ctx) {
            ctx.eax = yml.resolution.width;
            *reinterpret_cast<u32*>(ctx.ebp - 0xCC) = yml.resolution.height;
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
 * that has in game if any. With a single 1.0f that controlled the HUD we found some code around
 * GER.exe+1d510d4 which makes use of this value, and traced forward from there to the hook below.
 * The enclosing function (ger.exe+2110d4) is a single, generic UI-draw-command enqueue routine
 * shared by every UI element in the game - HUD frame, minimap, everything funnels through it to
 * submit a 16-float transform block. Reinterpreted as a standard affine 4x4 matrix, the diagonal
 * (indices 0/5/10) is the element's scale (X/Y/Z) and the last row (indices 12/13/14) is its
 * position (X/Y/Z).
 *
 * ctx.eax+0x30/+0x3C (the X and Y slots of that last row) are checked against the exact bit
 * patterns of -1.0f (0xBF80_0000) and +1.0f (0x3F80_0000) to find elements meant to span the
 * native-16:9 viewport edge-to-edge - everything else is left alone. Two distinct kinds of element
 * pass that check and are handled differently, told apart by whether ctx.eax+0x00 (the X scale)
 * starts at exactly 0.0f:
 *
 * - Centered, full-width elements (ctx.eax+0x00 starts at 0.0f - health bar frame, etc): here
 *   ctx.eax+0x30 isn't really a position, it's an "I span the full native-16:9 width" flag, and
 *   ctx.eax+0x00 is recomputed from scratch to re-center the element in the wider viewport:
 *     Reverse engineered equation for ctx.eax+0x00:
 *           1                    1                 2       nativeWidth
 *     ------------ * ------------------------ = ------- * -------------
 *      (width / 2)     (width / nativeWidth)     width        width
 *     if 3440x1440 (21:9): (2/3440) * (2560/3440) = 0.00058139535, unnormalized ~2311
 *     if 7680x2160 (32:9): (2/7680) * (3840/7680) = 1/7680 = 0.00013020833, unnormalized 7680
 *
 * - Corner-anchored elements (ctx.eax+0x00 starts non-zero, ~0.004 - the map/minimap background
 *   texture is the only known example): here ctx.eax+0x30 is a genuine X position (anchored to
 *   the native-16:9 left edge) and ctx.eax+0x00 is a genuine pre-existing scale, both living in a
 *   -1..1 NDC space sized for a 16:9 viewport. Scaling both by the same `ratio` used above maps
 *   that whole coordinate space onto the 16:9 region now centered in the wider viewport - resizing
 *   and repositioning it correctly without needing to know anything else about what it is.
 *
 * There's no type tag anywhere in this data to identify which of the two cases a given element is;
 * "does ctx.eax+0x00 start at exactly 0.0f" is the only signal found - empirically, by logging
 * every distinct match during gameplay - that reliably tells them apart.
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
            f32 valueAtZero = *reinterpret_cast<f32*>(ctx.eax + 0x00);
            bool isMapBackground = valueAtZero != 0.0f;
            if (((scaler0 & 0xBF800000) == 0xBF800000) && ((scaler1 & 0x3F800000) == 0x3F800000)) {
                f32 ratio = static_cast<f32>(nativeWidth) / static_cast<f32>(yml.resolution.width);
                if (isMapBackground) {
                    // Unlike the branch below, ctx.eax+0x30 here is an X position (anchored to
                    // the native-16:9 left edge), not a "full width" flag. Both it and the scale
                    // at ctx.eax+0x00 live in a -1..1 NDC space sized for a 16:9 viewport. Scaling
                    // both by the same ratio maps that whole space onto the 16:9 region now
                    // centered in the wider viewport.
                    f32 valueAtThirty = *reinterpret_cast<f32*>(ctx.eax + 0x30);
                    *reinterpret_cast<f32*>(ctx.eax + 0x00) = valueAtZero * ratio;
                    *reinterpret_cast<f32*>(ctx.eax + 0x30) = valueAtThirty * ratio;
                } else {
                    *reinterpret_cast<f32*>(ctx.eax + 0x00) = (2.0f / static_cast<f32>(yml.resolution.width)) * ratio;
                    *reinterpret_cast<f32*>(ctx.eax + 0x30) = ratio * -1.0f;
                }
            }
        }
    );
}

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
 * The engine has a dedicated `MovieWin32 : IMovie` class - DirectShow-backed
 * (CoCreateInstance on the system FilterGraph, IGraphBuilder::RenderFile to build the playback
 * graph, IMediaControl::Run/Pause/Stop to drive it) - found via its RTTI type descriptor string
 * (".?AVMovieWin32@@"). Its vtable sits at ger.exe+22b0c; walking the surrounding function
 * pointers and decompiling each (see docs/RE_LOG.md for the full dump and reasoning) gives a
 * small, clean, self-consistent layout:
 *   index 0 (+0x00) - scalar deleting destructor (confirms the vtable base - textbook MSVC
 *                     pattern: resets the vtable ptr to the base class, frees if requested)
 *   index 5 (+0x14) - Open(OpenParams*)   - builds the DirectShow graph, calls RenderFile
 *   index 6 (+0x18) - Close()             - calls Stop() (index 8) first, then releases every
 *                     COM interface the graph produced
 *   index 8 (+0x20) - Stop()              - IMediaControl::Stop(), but only if a movie is
 *                     actually loaded (see below - this matters a lot)
 *   index 9 (+0x24) - SetPaused(bool)     - IMediaControl::Pause()/Run() depending on the bool
 *   index 11 (+0x2c)- IsPaused()          - IMediaControl::GetState(), checks for State_Paused
 *   index 12 (+0x30)- Update()            - per-frame tick; applies a pending pause/resume via
 *                     index 9 and checks state via index 11
 *
 * `Open()` is hooked at its entry for movie *start* - it's called exactly once per movie, before a
 * single frame of it is ever decoded or rendered.
 *
 * `Close()` is not usable for movie *finished*, even though it looks like the obvious choice:
 * `Open()`'s first real action (right after its SEH prologue) is `CALL dword ptr [EAX+0x18]` on
 * its own vtable - it unconditionally calls `Close()` on itself first, as a defensive "tear down
 * whatever was open before" reset. So `Close()` fires on every movie's *start* too, not just when
 * one actually ends.
 *
 * `Stop()` is what signals "finished" - but only the part of it gated by a real `IMediaControl`
 * being cached:
 *   01821d57  PUSH ESI
 *   01821d58  MOV ESI,ECX
 *   01821d5a  CMP dword ptr [ESI+0x8],0x0      ; is an IMediaControl actually cached?
 *   01821d5e  JZ 0x01821d78                    ; if not, skip straight to a self+0x40 no-op
 *   ...
 *   01821d6f  MOV EAX,dword ptr [ESI+0x8]      ; <- hooked here
 *   01821d72  PUSH EAX
 *   01821d73  MOV ECX,dword ptr [EAX]
 *   01821d75  CALL dword ptr [ECX+0x24]        ; the real IMediaControl::Stop() call
 *   01821d78  ...
 * `Stop()` only calls the real `IMediaControl::Stop()` when `[this+0x8]` (the cached
 * `IMediaControl` pointer) is non-null - i.e. only when a movie was genuinely open and playing. On
 * the defensive `Open()`-triggered `Close()`-triggered `Stop()` that happens before any movie has
 * ever opened, that pointer is still null, so this call is skipped entirely. Reaching `01821d6f`
 * at all already proves a real movie was open, so hooking there catches every genuine "movie has
 * finished", whether reached via an explicit stop/close elsewhere or via the next movie's `Open()`
 * defensively closing this one.
 *
 * Neither hook alters anything either function actually does; both just read/write our own
 * `isMoviePlaying` global. No Windows API is involved anywhere in this fix.
 *
 * Known limitation: back-to-back movies with no gameplay in between would see the *next* movie's
 * `Open()` (sets isMoviePlaying=true) immediately followed by its own defensive Close()->Stop()
 * chain correctly stopping the *previous* movie (sets isMoviePlaying=false) - momentarily leaving
 * isMoviePlaying=false while the next movie is in fact already playing, until something else
 * flips it back. Not observed in practice and cosmetic at worst (HUD briefly not constrained).
 *
 * @return void
 */
void moviesFix() {
    Utils::SignatureHook openHook(
        "55 89 E5    6A FF    68 ?? ?? ?? ??    64 A1 00 00 00 00    50    81 EC 3C 01 00 00"
        "    A1 ?? ?? ?? ??    31 E8    89 85 F0 FF FF FF"
        "    53    56    57    50    8D 85 F4 FF FF FF    64 A3 00 00 00 00"
        "    89 CB    89 9D DC FE FF FF    8B 45 08    6A 00"
    );
    Utils::SignatureHook realStopHook(
        "56 89 CE 83 7E 08 00 74 18 8B 4E 1C 85 C9 74 08 8B 01 FF 90 80 00 00 00 8B 46 08 50 8B 08 FF 51 24",
        24
    );

    bool enable = yml.masterEnable;
    Utils::injectHook(enable, module, openHook,
        [](SafetyHookContext& ctx) {
            isMoviePlaying = true;
        }
    );
    Utils::injectHook(enable, module, realStopHook,
        [](SafetyHookContext& ctx) {
            isMoviePlaying = false;
        }
    );
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
    configResolutionFix();
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
