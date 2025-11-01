# GOD EATER RESSURRECTION Fix and GOD EATER 2 Rage Burst Fix
![GitHub Downloads (all assets, all releases)](https://img.shields.io/github/downloads/PolarWizard/GodEater1-2Fix/total)

Adds support for ultrawide resolutions and additional features.

***This project is designed exclusively for Windows due to its reliance on Windows-specific APIs. The build process requires the use of PowerShell.***

## Fixes
- Adds ultrawide support
- Unstretches and constrains movies back to 16:9

## Features
- Ability to constrain HUD to 16:9

## Build and Install
### Using CMake
1. Build and install:
```ps1
git clone https://github.com/PolarWizard/GodEater1-2Fix.git
cd GodEater1-2Fix; mkdir build; cd build
# If install is not needed you may omit -DCMAKE_INSTALL_PREFIX and cmake install step.
cmake -DCMAKE_GENERATOR_PLATFORM=Win32 -DCMAKE_INSTALL_PREFIX=<FULL-PATH-TO-GAME-FOLDER> ..
cmake --build .
cmake --install .
```
`cmake ..` will attempt to find the game folder in `C:/Program Files (x86)/Steam/steamapps/common/`. If the game folder cannot be found rerun the command providing the path to the game folder:<br>`cmake .. -DGAME_FOLDER="<FULL-PATH-TO-GAME-FOLDER>"`

2. Download [dinput8.dll](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases) Win32 version
3. Extract to game root folder: `GOD EATER RESURRECTION` and/or `GOD EATER 2 Rage Burst`

### Using Release
1. Download and follow instructions in [latest release](https://github.com/PolarWizard/GodEater1-2Fix/releases)

## Configuration
- Adjust settings in `<GOD EATER RESURRECTION;GOD EATER 2 Rage Burst>/scripts/GodEater1-2Fix.yml`

## Screenshots
| GOD EATER RESURRECTION |
| --- |
| ![Demo1](images/GodEater1-2Fix_1.gif) |
| <p align='center'> Fix disabled → Fix enabled </p> |

| GOD EATER 2 Rage Burst |
| --- |
| ![Demo2](images/GodEater1-2Fix_2.gif) |
| <p align='center'> Fix disabled → Fix enabled </p> |

## License
Distributed under the MIT License. See [LICENSE](LICENSE) for more information.

## External Tools
- [safetyhook](https://github.com/cursey/safetyhook)
- [spdlog](https://github.com/gabime/spdlog)
- [Ultimate ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader)
- [yaml-cpp](https://github.com/jbeder/yaml-cpp)
- [zydis](https://github.com/zyantific/zydis)
