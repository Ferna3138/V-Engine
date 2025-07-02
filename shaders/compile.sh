#!/bin/bash
set -e

echo "Compiling all shaders in the current directory..."

# Gather all supported shader files
shaders=$(ls *.vert *.frag *.comp *.geom *.tesc *.tese 2>/dev/null || true)

# Loop through each one
for shader in $shaders; do
    [ -e "$shader" ] || continue

    base_name="${shader%.*}"

    echo "Compiling $shader..."
    /usr/bin/glslc "$shader" -o "$base_name.spv"
    echo "$shader compiled successfully to $base_name.spv"
done

echo "Shader compilation completed."