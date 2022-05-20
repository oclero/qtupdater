#!/bin/bash

declare build_dir="./build"
declare qt5_dir="C:/Qt/5.15.2/msvc2019_64/lib/cmake/Qt5"
declare generator="Visual Studio 17 2022"
declare vcpkg_toolchain_file="C:/Users/ocler/Documents/Development/vcpkg/scripts/buildsystems/vcpkg.cmake"
declare vcpkg_triplet="x64-windows"
git submodule update --init --recursive
cmake -B $build_dir -G "$generator" -DQt5_DIR=$qt5_dir -DCMAKE_TOOLCHAIN_FILE="$vcpkg_toolchain_file" -DVCPKG_TARGET_TRIPLET=$vcpkg_triplet -DVCPKG_APPLOCAL_DEPS=OFF
