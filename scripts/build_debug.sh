#!/bin/bash
echo "Building with debug symbols..."

TELEMETRY_FLAG=""
OUTPUT_NAME="main"

# Parse flags
for arg in "$@"; do
  if [[ "$arg" == "-t" ]]; then
    TELEMETRY_FLAG="-DTELEMETRY_ENABLED"
    OUTPUT_NAME="main_telemetry"
    echo "→ Telemetry enabled"
  fi
done

mkdir -p bin

g++ -std=c++20 -pthread -Wall -Wextra -g -O0 \
  -I../include \
  src/main.cpp src/scheduler.cpp src/Task.cpp \
  -o bin/$OUTPUT_NAME \
  $TELEMETRY_FLAG

if [[ $? -ne 0 ]]; then
  echo "Build failed"
  exit 1
fi

echo "Build succeeded: bin/$OUTPUT_NAME"

# Choose a debugger: prefer gdb if present, otherwise use lldb
if command -v gdb &> /dev/null; then
  echo "Launching gdb on bin/$OUTPUT_NAME"
  exec gdb --args bin/$OUTPUT_NAME
elif command -v lldb &> /dev/null; then
  echo "gdb not found; launching lldb on bin/$OUTPUT_NAME"
  exec lldb bin/$OUTPUT_NAME
else
  echo "Neither gdb nor lldb found. Just run: bin/$OUTPUT_NAME"
  exit 0
fi