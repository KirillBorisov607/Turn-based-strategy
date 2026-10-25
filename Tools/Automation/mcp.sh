#!/bin/bash
# Usage: mcp.sh <tool_name> '<json arguments>'
TOOL="$1"; ARGS="${2:-{\}}"
BODY=$(printf '{"jsonrpc":"2.0","id":1,"method":"tools/call","params":{"name":"%s","arguments":%s}}' "$TOOL" "$ARGS")
curl -s -m 300 -X POST http://localhost:13579/mcp -H "Content-Type: application/json" --data-binary "$BODY"
