#!/bin/bash

declare vcpkg_dir="C:/Users/ocler/Documents/Development/vcpkg"
declare vcpkg_triplet="x64-windows"
$vcpkg_dir/vcpkg.exe install --triplet $vcpkg_triplet cpp-httplib
