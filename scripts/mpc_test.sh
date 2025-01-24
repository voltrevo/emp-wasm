#!/bin/bash

set -euo pipefail

# Define the programs to run
PROGRAM_A="./build/mpc 1 8005"
PROGRAM_B="./build/mpc 2 8005"
PROGRAM_C="./build/mpc 3 8005"
PROGRAM_D="./build/mpc 4 8005"

# Run 3 instances of the program in the background and print output as it comes
$PROGRAM_A 2>&1 | sed 's/^/A: /' &
PID1=$!
$PROGRAM_B 2>&1 | sed 's/^/B: /' &
PID2=$!
$PROGRAM_C 2>&1 | sed 's/^/C: /' &
PID3=$!
$PROGRAM_D 2>&1 | sed 's/^/D: /' &
PID4=$!

# Function to abort everything if a process fails
abort() {
  echo "Aborting..."
  kill $PID1 $PID2 $PID3 $PID4 2>/dev/null
  wait $PID1 $PID2 $PID3 $PID4 2>/dev/null
  exit 1
}

# Wait for all processes to complete, abort if any fail
wait $PID1 || abort
wait $PID2 || abort
wait $PID3 || abort
wait $PID4 || abort

echo "Finished"
