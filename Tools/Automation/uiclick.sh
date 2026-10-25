#!/bin/bash
# Usage: uiclick.sh <fx> <fy> - presses and releases the left button over a viewport point, a few frames apart
echo "$1 $2 down" > /c/ProjectsUE/_tools/frac.txt; /c/ProjectsUE/_tools/py.sh /c/ProjectsUE/_tools/press_frac.py
sleep 0.3
echo "$1 $2 up" > /c/ProjectsUE/_tools/frac.txt; /c/ProjectsUE/_tools/py.sh /c/ProjectsUE/_tools/press_frac.py
