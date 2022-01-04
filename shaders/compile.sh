#!/bin/bash

set -e

for file in `ls *.vert`
do
    ${VULKAN_SDK}/bin/glslc.exe $file -o ../data/$file.spv
done

for file in `ls *.frag`
do
    ${VULKAN_SDK}/bin/glslc.exe $file -o ../data/$file.spv
done