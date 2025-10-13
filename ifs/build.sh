#!/usr/bin/bash

output=../x64/ifs
sources="./*.cpp ../../_src/glad.c ../../_src/imgui/*.cpp"
includes="-I ../../_headers -I ../../_headers/imgui -I ../../_headers/nfd"
libs="-lOpenCL -lglfw -lGL -lpthread -ldl -lX11 -lXrandr -lXi -L ../../_lib -lnfd"

g++ -Wall -Wextra -o $output $sources $includes $libs `pkg-config --cflags --libs gtk+-3.0`
