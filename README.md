# **TypeIt**

**TypeIt** is a terminal-based touch typing game designed to test your typing skills. The game offers three base levels of difficulty. However, to keep things interesting, you can input custom text files for a personalized experience! While typing, you can see your accuracy and words per minute in real-time. The application is built with an interactive interface using the **FTXUI** library.

## **Technologies**

- **Language**: [C++20](https://isocpp.org/)
- **Testing Framework**: [Google Test](https://github.com/google/googletest)
- **Build System**: [CMake](https://cmake.org/)
- **Build Tool**: [Ninja](https://ninja-build.org/)
- **Package Management**: [vcpkg](https://github.com/microsoft/vcpkg)
- **UI Library**: [FTXUI](https://github.com/ArthurSonzogni/FTXUI)

## **Features**

- **Touch Typing Test**: Test your typing capabilities.
- **Levels**: Three difficulty levels (Simple, Medium, Hard) based on word frequency and length.
- **Custom Files**: Input your own textual files for personalized typing tests.
- **Predefined Timer**: Choose from 15s, 30s, or 60s test timers.
- **Custom Timer**: Define your own custom test session duration.
- **Statistics**: Track your typing accuracy and speed in real-time.
- **Error Check**: Incorrect characters are highlighted during typing.

## **Requirements**

Before building and running the project, ensure you have the following tools installed:

- **Git**: To clone the repository.
- **CMake >= 3.15**: To configure the project and manage dependencies.
- **Ninja**: The build tool used in this project.
- **GCC/G++**: C and C++ compilers for Linux.
- **CL (MSVC)**: C and C++ compiler for Windows.

> **Note**: The C++ compiler must support the C++20 standard.

## **Installation**

****

### **Initialize Git Submodule**

Regardless of whether you're on Windows or Linux, you must initialize the Git submodule:

```bash
   git submodule update --init --recursive
```

****

### *LINUX*

Verify the installation of the required tools:

```bash
    git --version
    cmake --version
    ninja --version
    gcc --version
    g++ --version
```

*Setup CMake:*
```bash
  cmake -DCMAKE_MAKE_PROGRAM=ninja -DCMAKE_TOOLCHAIN_FILE=./external/vcpkg/scripts/buildsystems/vcpkg.cmake -G Ninja -S . -B cmake-build -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++
```
*Build The Project:*
```bash
  cmake --build cmake-build 
```
*Add The Executable To Games Folder For Easier Access:*
```bash
  sudo ln -sfv "cmake-build/src/TypeIt" "/usr/games/TypeIt"
```
*Start The Game:*
```bash
  TypeIt
```
****
