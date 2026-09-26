# 1. Create the shell wrapper script
cat << 'EOF' > grrasm
#!/bin/sh
exec java -jar "$0" "$@"
EOF

# 2. Append your IntelliJ JAR artifact
cat /mnt/c/Users/gmtev/Documents/GitHub/grrvm/assembler/out/artifacts/grrasm/grrasm.jar >> grrasm

# 3. Make it executable
chmod +x grrasm