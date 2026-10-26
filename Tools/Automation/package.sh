#!/bin/bash
# Packages the game for Windows into C:/ProjectsUE/_build (close the editor first: its MCP port breaks the cook).
"/c/Program Files/Epic Games/UE_5.8/Engine/Build/BatchFiles/RunUAT.bat" BuildCookRun -project="C:/ProjectsUE/Turn-based-strategy/The_battle_code/The_battle_code.uproject" \
  -platform=Win64 -clientconfig=Development -build -cook -stage -pak -archive -archivedirectory="C:/ProjectsUE/_build" -noP4 -utf8output -unattended > /c/ProjectsUE/_logs/package_last.log 2>&1
echo "exit $?"; grep -E "BUILD SUCCESSFUL|BUILD FAILED|Error:" /c/ProjectsUE/_logs/package_last.log | tail -5
