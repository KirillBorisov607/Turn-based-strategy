import unreal
gw=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
unreal.KeepersTutorialSubsystem.start_tutorial(gw)
print("tutorial")
