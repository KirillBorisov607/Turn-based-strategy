import unreal
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
unreal.KeepersCampaignSubsystem.set_mercenary_battle(world, True)
print('flag set')
