import unreal,json,pathlib
root=pathlib.Path(__file__).resolve().parents[1]
assets=json.loads((root/'Docs/ResidentIdle/import.json').read_text())
mesh=unreal.load_asset(next(a['path'] for a in assets if a['type']=='SkeletalMesh'))
animation=unreal.load_asset(next(a['path'] for a in assets if a['type']=='AnimSequence'))
if abs(animation.sequence_length-6)>0.05: raise RuntimeError('Animation duration invalid')
for path in unreal.EditorAssetLibrary.list_assets('/Game/Coursework/ResidentAnimated',recursive=True):
    material=unreal.load_asset(path)
    if isinstance(material,unreal.Material):
        unreal.MaterialEditingLibrary.set_material_usage(material,unreal.MaterialUsage.MATUSAGE_SKELETAL_MESH)
        unreal.MaterialEditingLibrary.recompile_material(material)
system=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
report=[]
for map_path in ['/Game/HouseFurniture/Maps/Demo_01_Night','/Game/HouseFurniture/Maps/Demo_01']:
    world=unreal.EditorLoadingAndSavingUtils.load_map(map_path)
    for a in system.get_all_level_actors():
        if a.get_actor_label().startswith('Coursework_Resident'): system.destroy_actor(a)
    actor=system.spawn_actor_from_class(unreal.SkeletalMeshActor,unreal.Vector(-2335,9925,60.75),unreal.Rotator(yaw=-90))
    actor.set_actor_label('Coursework_Resident_Harper_Idle')
    actor.tags=['CourseworkResidentBody','CourseworkResidentIdle']
    component=actor.skeletal_mesh_component
    component.set_skeletal_mesh(mesh)
    if component.get_num_bones()<50: raise RuntimeError('Incomplete imported skeleton')
    component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    component.override_animation_data(animation,True,True,0.0,1.0)
    component.set_editor_property('visibility_based_anim_tick_option',unreal.VisibilityBasedAnimTickOption.ALWAYS_TICK_POSE_AND_REFRESH_BONES)
    if not unreal.EditorLoadingAndSavingUtils.save_map(world,map_path): raise RuntimeError('Failed map save')
    report.append({'map':map_path,'mesh':mesh.get_path_name(),'animation':animation.get_path_name(),'length':animation.sequence_length,'location':[-2335,9925,60.75],'bones':[str(component.get_bone_name(i)) for i in range(component.get_num_bones())]})
for unused_path in ['/Game/Coursework/ResidentIdle','/Game/Coursework/ResidentIdleReady']:
    if unreal.EditorAssetLibrary.does_directory_exist(unused_path): unreal.EditorAssetLibrary.delete_directory(unused_path)
unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True,True)
(root/'Docs/ResidentIdle/placement.json').write_text(json.dumps(report,indent=2),encoding='utf8')
unreal.log('IDLE_RESIDENT_PLACED')
