#!/bin/bash
# Starts the Unreal Editor with the project and waits for the MCP server
"/c/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe" "C:/ProjectsUE/Turn-based-strategy/The_battle_code/The_battle_code.uproject" -log > /c/ProjectsUE/_logs/editor_run.log 2>&1 &
for i in $(seq 1 120); do
  if curl -s -m 2 -X POST http://localhost:13579/mcp -H "Content-Type: application/json" -d '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{}}' | grep -q serverInfo; then echo "editor ready after $((i*5))s"; exit 0; fi
  sleep 5
done
echo "editor did not start"; exit 1
