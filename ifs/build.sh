#!/usr/bin/bash

output=../x64/ifs
sources="./*.cpp ../../../cpp/_src/glad.c ../../../cpp/_src/imgui/*.cpp"
includes="-I ../../_headers -I ../../../cpp/_headers -I ../../../cpp/_headers/imgui -I ../../../cpp/_headers/nfd"
libs="-lOpenCL -lglfw -lGL -lpthread -ldl -lX11 -lXrandr -lXi -L ../../../cpp/_lib -lnfd"

g++ -o $output $sources $includes $libs `pkg-config --cflags --libs gtk+-3.0`
