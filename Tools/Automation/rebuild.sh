#!/bin/bash
# Quits the editor, rebuilds the project's C++ plugins and starts the editor again.
/c/ProjectsUE/_tools/py.sh /c/ProjectsUE/_tools/pie_stop.py > /dev/null 2>&1
printf 'import unreal\nunreal.SystemLibrary.quit_editor()\n' > /c/ProjectsUE/_tools/quit.py
/c/ProjectsUE/_tools/py.sh /c/ProjectsUE/_tools/quit.py > /dev/null 2>&1
for i in $(seq 1 60); do tasklist | grep -q "UnrealEditor.exe" || break; sleep 2; done
cd /c/ProjectsUE && "/c/Program Files/Epic Games/UE_5.8/Engine/Build/BatchFiles/Build.bat" The_battle_codeEditor Win64 Development -Project="C:/ProjectsUE/Turn-based-strategy/The_battle_code/The_battle_code.uproject" -WaitMutex > /c/ProjectsUE/_logs/build_last.log 2>&1
R=$?
grep -E "error C|error :|Result:" /c/ProjectsUE/_logs/build_last.log | cut -c1-300 | head -20
[ $R -ne 0 ] && exit $R
/c/ProjectsUE/_tools/start_editor.sh
