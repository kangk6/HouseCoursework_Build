import unreal, json, pathlib

DEST='/Game/Coursework/ResidentRefined'
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
meshes=[unreal.load_asset(p) for p in unreal.EditorAssetLibrary.list_assets(DEST, recursive=True)]
meshes=[m for m in meshes if isinstance(m,unreal.StaticMesh)]
if len(meshes)!=9: raise RuntimeError('Unexpected resident parts: '+str(len(meshes)))
report=[]
for map_path in ['/Game/HouseFurniture/Maps/Demo_01_Night','/Game/HouseFurniture/Maps/Demo_01']:
    world=unreal.EditorLoadingAndSavingUtils.load_map(map_path)
    for a in actors.get_all_level_actors():
        if a.get_actor_label().startswith('Coursework_Resident'): actors.destroy_actor(a)
    parts=[]
    for mesh in meshes:
        a=actors.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(-2335,9925,60.96),unreal.Rotator(yaw=-90))
        a.set_actor_label('Coursework_Resident_'+mesh.get_name())
        c=a.static_mesh_component
        c.set_mobility(unreal.ComponentMobility.MOVABLE)
        c.set_static_mesh(mesh)
        c.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        if mesh.get_name()=='Avery___anatomical_body': a.tags=['CourseworkResidentBody']
        parts.append({'label':a.get_actor_label(),'mesh':mesh.get_path_name(),'location':str(a.get_actor_location()),'bounds':str(a.get_actor_bounds(False))})
    if not unreal.EditorLoadingAndSavingUtils.save_map(world,map_path): raise RuntimeError('Map save failed')
    report.append({'map':map_path,'parts':parts,'height_cm':176,'feet_z':60.96})
unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True,True)
pathlib.Path('D:/House_assets/ResidentRefined/placement.json').write_text(json.dumps(report,indent=2),encoding='utf8')
unreal.log('REFINED_RESIDENT_PLACED')
