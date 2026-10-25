#!/bin/bash
# Usage: gen_shoot.sh <seed> <biome index> <name> - plays L_Generated with that seed and biome, shoots the field from above
echo "$1 $2" > /c/ProjectsUE/_tools/gen_arg.txt
echo "$3" > /c/ProjectsUE/_tools/gen_name.txt
/c/ProjectsUE/_tools/py.sh /c/ProjectsUE/_tools/gen_next.py > /dev/null
echo "/Game/Maps/Generated/L_Generated" > /c/ProjectsUE/_tools/pie_level.txt
/c/ProjectsUE/_tools/py.sh /c/ProjectsUE/_tools/pie_start.py > /dev/null
sleep 25
/c/ProjectsUE/_tools/py.sh /c/ProjectsUE/_tools/gen_top.py
sleep 4
/c/ProjectsUE/_tools/py.sh /c/ProjectsUE/_tools/pie_stop.py > /dev/null
sleep 3
