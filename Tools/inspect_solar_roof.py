"""Read-only editor inspection; never saves the source map or assets."""
import unreal, json, pathlib
unreal.EditorLoadingAndSavingUtils.load_map('/Game/HouseFurniture/Maps/Demo_01_Night')
rows=[]
for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
    for c in a.get_components_by_class(unreal.StaticMeshComponent):
        m=c.static_mesh
        if not m: continue
        origin,extent,_=unreal.SystemLibrary.get_component_bounds(c)
        if not (-3200<origin.x<-1000 and 8700<origin.y<11200 and origin.z>600): continue
        rows.append(dict(actor=a.get_name(),component=c.get_name(),mesh=m.get_path_name(),origin=[origin.x,origin.y,origin.z],extent=[extent.x,extent.y,extent.z],transform=str(c.get_world_transform())))
p=pathlib.Path('D:/HouseCoursework_Team/Saved/Acceptance/SolarEnergy');p.mkdir(parents=True,exist_ok=True)
(p/'roof_inspection.json').write_text(json.dumps(rows,indent=2),encoding='utf8')
unreal.log('SOLAR_ROOF_INSPECTION_DONE')
