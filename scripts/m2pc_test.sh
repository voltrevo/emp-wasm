#!/bin/bash

set -euo pipefail

# Define the programs to run
PROGRAM_A="./build/m2pc 1 8005"
PROGRAM_B="./build/m2pc 2 8005"

# Run 3 instances of the program in the background and print output as it comes
$PROGRAM_A 2>&1 | sed 's/^/A: /' &
PID1=$!
$PROGRAM_B 2>&1 | sed 's/^/B: /' &
PID2=$!

# Function to abort everything if a process fails
abort() {
  echo "Aborting..."
  kill $PID1 $PID2 2>/dev/null
  wait $PID1 $PID2 2>/dev/null
  exit 1
}

# Wait for all processes to complete, abort if any fail
wait $PID1 || abort
wait $PID2 || abort

echo "Finished"
