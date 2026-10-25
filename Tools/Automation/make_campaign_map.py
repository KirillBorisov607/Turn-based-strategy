import unreal
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
dst = '/Game/Maps/Campaign/L_CampaignMap'
if not unreal.EditorAssetLibrary.does_asset_exist(dst):
    les.load_level('/Game/Maps/L_MainMenu')
    print('copy', unreal.EditorLoadingAndSavingUtils.save_map(ues.get_editor_world(), dst))
les.load_level(dst)
for a in eas.get_all_level_actors():
    if a.get_class().get_name() == 'BP_MainMenu_C':
        eas.destroy_actor(a)
        print('removed main menu actor')
print('saved', les.save_current_level())
