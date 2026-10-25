#!/bin/bash
# Usage: record.sh <map> <clip name> <duration s> "<steps>" - records a clip in a standalone game window (KeepersDirector)
OUT=C:/ProjectsUE/_logs/rec/$2
rm -rf "/c/ProjectsUE/_logs/rec/$2"; mkdir -p "/c/ProjectsUE/_logs/rec/$2"
MSYS2_ARG_CONV_EXCL="*" "/c/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe" "C:/ProjectsUE/Turn-based-strategy/The_battle_code/The_battle_code.uproject" "$1" -game -windowed -ResX=1280 -ResY=720 -WinX=40 -WinY=40 \
  -KeepersRecord=$OUT -KeepersDuration=$3 -KeepersWarmup=${WARMUP:-2} "-KeepersSteps=$4" -abslog=C:/ProjectsUE/_logs/rec/$2.log > /dev/null 2>&1
echo "$2: $(ls /c/ProjectsUE/_logs/rec/$2 | wc -l) frames, errors=$(grep -c 'Error' /c/ProjectsUE/_logs/rec/$2.log)"
