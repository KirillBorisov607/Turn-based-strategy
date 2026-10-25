import unreal
src = '/Game/Art/Units/Warrior/TX_Icon_Warrior'
for n in ['Bandit', 'BanditArcher', 'Brute']:
    dst = '/Game/Art/Units/Warrior/TX_Icon_%s' % n
    if not unreal.EditorAssetLibrary.does_asset_exist(dst):
        unreal.EditorAssetLibrary.duplicate_asset(src, dst)
    print(dst, unreal.EditorAssetLibrary.save_asset(dst, False))
print(unreal.EditorAssetLibrary.save_asset('/Game/Blueprints/Core/Units/Utilities/DT_DefaultUnitDataPerType', False))
print(unreal.EditorAssetLibrary.save_asset('/Game/Art/Units/E_UnitType', False))
