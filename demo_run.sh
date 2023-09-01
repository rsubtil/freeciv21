#!/bin/sh

match=alpha

# Test if savefiles and logs directory exists
if [ ! -d "savefiles" ]; then
    echo "Savefiles directory not found"
    exit 1
fi
if [ ! -d "logs" ]; then
    echo "Logs directory not found"
    exit 1
fi

if [ -z "$1" ]; then
  if [ "$(ls -A savefiles)" ]; then
    echo "Savefiles directory not empty! Specify last save_file"
    exit 1
  fi
    echo "No scenario file specified, using scenario.sav"
    scenario=scenario.sav
else
    scenario=$1
fi

# We are in the right place
lunar_gambit-server -f "$scenario" --saves savefiles --log ../logs/$(date +%d_%m___%H_%M_%S).log --read setup_commands

# Send an email everytime this script leaves, to warn us
echo "Lunar Gambit server stopped"
