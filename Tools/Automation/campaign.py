# Usage via file: first line of C:/ProjectsUE/_tools/campaign_cmd.txt = command (start|state|enter N|win_pick|complete)
import unreal
gw = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()

camp = unreal.KeepersCampaignSubsystem.get_campaign(gw)
cmd = open(r'C:/ProjectsUE/_tools/campaign_cmd.txt').read().split()
if cmd[0] == 'start':
    camp.start_new_campaign()
    print('started')
elif cmd[0] == 'state':
    print('world', gw.get_name(), 'active', camp.is_active(), 'available', list(camp.get_available_nodes()))
elif cmd[0] == 'enter':
    camp.enter_node(int(cmd[1]))
    print('entered', cmd[1])
