#!/bin/bash

# Enable strict error handling
set -e

# Get directory of the script
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if [[ "$(uname)" == "Darwin" ]]; then
    GLSLC="$SCRIPT_DIR/../../../Dependencies/shader_compile/Mac/glslc"
else
    GLSLC="$SCRIPT_DIR/../../../Dependencies/shader_compile/glslc"
fi

# Shader file extensions to compile
EXTENSIONS=("vert" "frag" "rgen" "rchit" "rmiss" "comp")

echo "Compiling shaders in $SCRIPT_DIR..."

# Loop through each extension and compile matching files
for EXT in "${EXTENSIONS[@]}"; do
    for FILE in "$SCRIPT_DIR"/*.$EXT; do
        [ -e "$FILE" ] || continue  # Skip if no match
        BASENAME=$(basename "$FILE")
        echo "Compiling $BASENAME..."
        "$GLSLC" "$FILE" -o "$FILE.spv" || {
            echo "Failed to compile $BASENAME"
            exit 1
        }
    done
done

echo "Shader compilation complete."
