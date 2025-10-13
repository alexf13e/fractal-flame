#!/usr/bin/bash

./build.sh

if [ $? -eq 0 ]; then
    ./nvrun.sh
fi
