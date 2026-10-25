import unreal
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
les.new_level('/Temp/TT_Lineup') if False else None
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
# clean previous
for a in eas.get_all_level_actors():
    if a.get_actor_label().startswith('TTL_'): a.destroy_actor()
P='/Game/Toony_Tiny_RTS_Set/Meshes/Characters_Parts/Units/'
import os
bodies = sorted(set(a.split('.')[0].split('/')[-1] for a in unreal.EditorAssetLibrary.list_assets(P,False,False) if 'Body' in a))
heads = sorted(set(a.split('.')[0].split('/')[-1] for a in unreal.EditorAssetLibrary.list_assets(P,False,False) if 'Head' in a))
mode = MODE
items = bodies if mode=='body' else heads
base = unreal.Vector(0, 20000, 5000)
cols = 12
for i,n in enumerate(items):
    x = (i % cols) * 120; y = (i // cols) * 260
    loc = base + unreal.Vector(x, 0, -y)
    act = eas.spawn_actor_from_class(unreal.SkeletalMeshActor, loc, unreal.Rotator(0,0,0))
    act.set_actor_label('TTL_'+n)
    c = act.skeletal_mesh_component
    if mode=='body':
        c.set_skeletal_mesh_asset(unreal.load_asset(P+n))
        h = act.add_component_by_class(unreal.SkeletalMeshComponent, False, unreal.Transform(), False) if hasattr(act,'add_component_by_class') else None
    else:
        c.set_skeletal_mesh_asset(unreal.load_asset(P+n))
    t = eas.spawn_actor_from_class(unreal.TextRenderActor, loc + unreal.Vector(0, 0, -30), unreal.Rotator(0,-90,0))
    t.set_actor_label('TTL_t'+n)
    t.text_render.set_text(n.replace('Unit_',''))
    t.text_render.set_world_size(14)
    t.text_render.set_horizontal_alignment(unreal.HorizTextAligment.EHTA_CENTER)
print(len(items))
