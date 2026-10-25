#!/bin/bash
# Usage: vshot.sh <out.jpg> - editor viewport screenshot via MCP, saved to a file
./mcp.sh take_screenshot "{\"width\":1920,\"height\":1080,\"quality\":90}" > /c/ProjectsUE/_tools/vshot.json
python -c "
import json,base64,sys
d=json.load(open(r'C:/ProjectsUE/_tools/vshot.json',encoding='utf-8'))
for c in d['result']['content']:
    if c['type']=='image': open(sys.argv[1],'wb').write(base64.b64decode(c['data']))
" "$1"
