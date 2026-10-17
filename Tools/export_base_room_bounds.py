import unreal,json,pathlib
unreal.EditorLoadingAndSavingUtils.load_map('/Game/HouseFurniture/Maps/Demo_01_Night')
rows=[]
for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
    for c in a.get_components_by_class(unreal.StaticMeshComponent):
        if not c.static_mesh: continue
        p,e,r=unreal.SystemLibrary.get_component_bounds(c)
        if -2600<p.x<-1500 and 9000<p.y<10800 and 50<p.z<540:
            rows.append({'actor':a.get_actor_label(),'component':c.get_name(),'mesh':c.static_mesh.get_name(),'center':[p.x,p.y,p.z],'extent':[e.x,e.y,e.z]})
pathlib.Path('D:/HouseCoursework/Saved/BaseRoomBounds.json').write_text(json.dumps(rows,indent=2))
