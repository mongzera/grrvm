#!/usr/bin/env bash

set -e

rm -rf bin
mkdir bin
# 1. Create the shell wrapper script
cat << 'EOF' > bin/grrasm
#!/bin/sh
exec java -jar "$0" "$@"
EOF

# 2. Append your IntelliJ JAR artifact

cat out/artifacts/grrasm/grrasm.jar >> bin/grrasm

# 3. Make it executable
chmod +x bin/grrasm