# How to build

* Download LunarG Vulkan SDK [here](https://www.lunarg.com/vulkan-sdk/) and install it
* Git is required to fetch dependencies automatically
* CMake is the only supported build system so please use it
* On Ubuntu you may need those to configure CMakeList.txt without error
    ```
    sudo apt-get install libx11-dev
    sudo apt-get install xorg-dev libglu1-mesa-dev
    ```

## For Windows

* Build CMake target `dalbaragi_windows` and execute it

## For Linux

* I have only tested it on Ubuntu
* Build CMake target `dalbaragi_linux` and execute it

## For Android

* Open directory `{repo root}/app/android` on Android Studio and build it there