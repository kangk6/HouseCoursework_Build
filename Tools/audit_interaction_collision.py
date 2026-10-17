import unreal,json,pathlib
unreal.EditorLoadingAndSavingUtils.load_map('/Game/HouseFurniture/Maps/Demo_01_Night')
rows=[]
for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
    if not any(s in actor.get_actor_label() for s in ['Cabinets_A','ClothBasket','Teapod','Oven']): continue
    for c in actor.get_components_by_class(unreal.StaticMeshComponent):
        if not c.static_mesh: continue
        body=c.static_mesh.get_editor_property('body_setup')
        rows.append({'actor':actor.get_actor_label(),'component':c.get_name(),'mesh':c.static_mesh.get_path_name(),'collision':str(c.get_collision_enabled()),'visibility_response':str(c.get_collision_response_to_channel(unreal.CollisionChannel.ECC_VISIBILITY)),'trace_flag':str(body.get_editor_property('collision_trace_flag')),'double_sided':body.get_editor_property('double_sided_geometry'),'mesh_local_bounds':str(c.static_mesh.get_bounding_box()),'relative_rotation':str(c.get_editor_property('relative_rotation'))})
pathlib.Path('D:/HouseCoursework/Saved/InteractionCollisionAudit.json').write_text(json.dumps(rows,indent=2))
