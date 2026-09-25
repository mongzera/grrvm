#!/bin/bash

# Exit immediately if any command fails
set -e

# Remove old & Re-Create output directory
rm -rf bin
mkdir -p bin

# 1. Collect and compile all Java files.
# Pass explicit source paths so javac recognizes package roots properly.
echo "Compiling Java files..."
find . ../assembler -name "*.java" > sources.txt
javac -d bin @sources.txt
rm sources.txt

# 2. Automatically resolve the fully qualified main class name
# (Looks inside bin/ for where Assembler.class ended up, e.g., com/example/Assembler.class)
MAIN_CLASS_FILE=$(find bin -name "Assembler.class" | head -n 1)

if [ -z "$MAIN_CLASS_FILE" ]; then
    echo "Error: Assembler.class was not found in bin directory."
    exit 1
fi

# Convert path "bin/com/example/Assembler.class" -> "com.example.Assembler"
FULLY_QUALIFIED_CLASS=$(echo "$MAIN_CLASS_FILE" | sed -e 's|^bin/||' -e 's|\.class$||' -e 's|/|\.|g')

echo "Running $FULLY_QUALIFIED_CLASS..."

# 3. Run using the resolved package name
java -cp bin "$FULLY_QUALIFIED_CLASS" "$@"
