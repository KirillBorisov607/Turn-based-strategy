#!/bin/bash
# Usage: shot_game.sh <name> - screenshot of the running PIE with UI, cropped to the game viewport
rm -rf /c/ProjectsUE/Turn-based-strategy/The_battle_code/Saved/Screenshots
printf 'import unreal\ngw = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()\nunreal.SystemLibrary.execute_console_command(gw, "shot showui")\n' > /c/ProjectsUE/_tools/shot_ui.py
/c/ProjectsUE/_tools/py.sh /c/ProjectsUE/_tools/shot_ui.py > /dev/null
sleep 3
python -c "
from PIL import Image
im=Image.open(r'C:\ProjectsUE\Turn-based-strategy\The_battle_code\Saved\Screenshots\WindowsEditor\ScreenShot00000.png')
im.crop((620,160,1920,980)).save(r'C:\ProjectsUE\_logs\ui_$1.png')"
echo "C:/ProjectsUE/_logs/ui_$1.png"
