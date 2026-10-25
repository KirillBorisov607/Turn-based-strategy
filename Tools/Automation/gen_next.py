import unreal
a = open(r'C:/ProjectsUE/_tools/gen_arg.txt').read().split()
unreal.KeepersMapGenLibrary.set_next_generated_map(int(a[0]), list(unreal.KeepersBiome)[int(a[1])])
print('next', a)
