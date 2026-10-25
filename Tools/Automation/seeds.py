import unreal
for b in list(unreal.KeepersBiome):
    print(b, unreal.KeepersMapGenLibrary.check_seeds(b, 1, 50), '/ 50')
