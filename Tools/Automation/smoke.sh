#!/bin/bash
# Usage: smoke.sh <map path> <seconds> <name> - plays a level in PIE, takes an in-game screenshot, reports script errors
L=/c/ProjectsUE/Turn-based-strategy/The_battle_code/Saved/Logs/The_battle_code.log
START=$(wc -l < $L)
echo "$1" > /c/ProjectsUE/_tools/pie_level.txt
/c/ProjectsUE/_tools/py.sh /c/ProjectsUE/_tools/pie_start.py > /dev/null
sleep "$2"
sed -i "s#filename=[^']*#filename=C:/ProjectsUE/_logs/game_$3.png#" /c/ProjectsUE/_tools/pie_shot.py
/c/ProjectsUE/_tools/py.sh /c/ProjectsUE/_tools/pie_shot.py > /dev/null
sleep 3
/c/ProjectsUE/_tools/py.sh /c/ProjectsUE/_tools/pie_stop.py > /dev/null
sleep 2
tail -n +$START $L > /c/ProjectsUE/_logs/pie_$3.log
echo "$3: errors=$(grep -c 'PIE: Error' /c/ProjectsUE/_logs/pie_$3.log) script_warnings=$(grep -c 'LogScript: Warning' /c/ProjectsUE/_logs/pie_$3.log)"
grep 'PIE: Error' /c/ProjectsUE/_logs/pie_$3.log | sed -E 's/^\[[^]]*\]\[[^]]*\]//' | cut -c1-240 | sort | uniq -c | sort -rn | head -5
