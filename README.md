# TrafficSim

Traffic simulator

## How to setup C++ projects

1. Install Visual Studio Tools and, from there, install the MSVC C++ compiler with CMake. Alternatively, install gcc compiler and CMake separately.

2. Install C++ and CMake extensions on Visual Studio.

3. Create a new project that contains at least one `*.cpp` file and `CMakeList.txt`. The CMake extension should automatically recognize the project and attach the MSVC compiler to it.

## How to setup Qt

1. For a nice and modern GUI, we can use Qt. First of all, install Qt6 from the official website. Make sure to install the version that matches the C++ compiler you have installed.

2. Make sure that the Qt/bin installation is listed on PATH.

3. Now Qt `.dll` should be automatically picked up by the C++ project, if properly linked in the CMakeList.txt.

4. From now on, when deploying, remember to run

```bash
C:\Qt\6.11.1\msvc2022_64\bin\windeployqt.exe `
  --release `
  --qmldir .\src\gui `
  .\build\Release\TrafficSim.exe
```

in order to make the Release build self-contained, i.e. including all necessary Qt `.dll`.

## Build target

From the VS Code options, we can select the build target from `>CMake: select variant` and then choose between `Release` or `Debug`.
