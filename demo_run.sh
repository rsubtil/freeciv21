#!/bin/sh

logdir=$1
if [ -z "$logdir" ]; then
    echo "Usage: $0 <logdir>"
    exit 1
fi


logfile=$logdir/$(date +%d_%m___%H_%M_%S).log
echo "Storing logfile at $logfile"

# Run the demo
echo "Server running... (type q and enter to quit, or end from game)"
./build/freeciv21-server -f demoScenario.sav --saves ../spaceRaceScenario/backups > $logfile 2>&1

echo "Server closed. Logfile at $logfile"
