#!/bin/bash

# Requires ImageMagick
# https://imagemagick.org/

# Directory to search for files
INPUT_DIR="../Specular"

# Directory to save the output
OUTPUT_DIR="../rM"

# Convert paths to Windows-style paths if running on Windows
if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "win32" ]]; then
    INPUT_DIR=$(cygpath -w "$INPUT_DIR")
    OUTPUT_DIR=$(cygpath -w "$OUTPUT_DIR")
fi

# Find and process all files containing "Specular" in the name
for file in "$INPUT_DIR"/*Specular*.png; do
    if [ -f "$file" ]; then
        # Extract the base name of the image without extension
        BASENAME=$(basename "$file" | cut -d. -f1)
        FILENAME=$file
        
        # Output paths
        ROUGHNESS_OUTPUT="${OUTPUT_DIR}/r-${BASENAME}.png"
        METALNESS_OUTPUT="${OUTPUT_DIR}/M-${BASENAME}.png"
        
        # Convert paths to Windows-style paths if running on Windows
        if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "win32" ]]; then
            FILENAME=$(cygpath -w "$file")
            ROUGHNESS_OUTPUT=$(cygpath -w "$ROUGHNESS_OUTPUT")
            METALNESS_OUTPUT=$(cygpath -w "$METALNESS_OUTPUT")
        fi

        echo "Processing $FILENAME..."
        
        # Extract Roughness (Green Channel)
        magick "${FILENAME}" -channel G -separate "${ROUGHNESS_OUTPUT}"

        # Extract Metalness (Blue Channel)
        magick "${FILENAME}" -channel B -separate "${METALNESS_OUTPUT}"

        echo "Generated ${ROUGHNESS_OUTPUT} and ${METALNESS_OUTPUT}"
    else
        echo "No files found containing 'Specular' in their names."
    fi
done

echo "Processing complete!"