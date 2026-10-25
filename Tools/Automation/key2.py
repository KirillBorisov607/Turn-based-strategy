import unreal
k = open(r'C:/ProjectsUE/_tools/key_arg.txt').read().split()
print(unreal.KeepersInputSimulator.set_key(k[0], k[1] == '1'))
