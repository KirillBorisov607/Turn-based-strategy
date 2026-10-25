#!/bin/bash
# Usage: gshot.sh <name> - game frame (HighResShot) with the UMG layer drawn over it, no desktop capture
echo "$1" > /c/ProjectsUE/_tools/ui_shot.txt
rm -f /c/ProjectsUE/_logs/g_$1.png /c/ProjectsUE/_logs/u_$1.png
/c/ProjectsUE/_tools/py.sh /c/ProjectsUE/_tools/ui_shot.py
for i in $(seq 1 20); do [ -f /c/ProjectsUE/_logs/g_$1.png ] && break; sleep 1; done
sleep 1
python -c "
from PIL import Image
g=Image.open(r'C:\ProjectsUE\_logs\g_$1.png').convert('RGBA')
u=Image.open(r'C:\ProjectsUE\_logs\u_$1.png').convert('RGBA').resize(g.size)
Image.alpha_composite(g,u).convert('RGB').save(r'C:\ProjectsUE\_logs\s_$1.png')"
echo "C:/ProjectsUE/_logs/s_$1.png"
