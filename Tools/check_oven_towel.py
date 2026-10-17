import unreal,json,pathlib
unreal.EditorLoadingAndSavingUtils.load_map('/Game/HouseFurniture/Maps/Demo_01_Night')
rows=[]
for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
    if 'Towel' not in actor.get_name(): continue
    for c in actor.get_components_by_class(unreal.StaticMeshComponent):
        p,e,r=unreal.SystemLibrary.get_component_bounds(c)
        if -1800<p.x<-1600 and 9300<p.y<9450:
            parent=c.get_attach_parent()
            rows.append({'name':actor.get_name(),'label':actor.get_actor_label(),'position':[p.x,p.y,p.z],'parent':parent.get_path_name() if parent else None,'mobility':str(c.get_editor_property('mobility'))})
pathlib.Path('D:/HouseCoursework/Saved/OvenTowelAudit.json').write_text(json.dumps(rows,indent=2))
