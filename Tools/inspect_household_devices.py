"""Read-only inspection of household appliances and original material slots."""
import unreal,json,pathlib
unreal.EditorLoadingAndSavingUtils.load_map('/Game/HouseFurniture/Maps/Demo_01_Night')
rows=[]
for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
    for c in a.get_components_by_class(unreal.StaticMeshComponent):
        m=c.static_mesh
        if not m:continue
        text=(a.get_name()+' '+m.get_name()).lower()
        if not any(k in text for k in ['computer','monitor','television','fridge','aircon','condition','tv','screen']):continue
        origin,extent,_=unreal.SystemLibrary.get_component_bounds(c)
        if not (-2600<origin.x<-1500 and 9000<origin.y<10800 and 60<origin.z<650):continue
        mats=[]
        for i in range(c.get_num_materials()):
            mat=c.get_material(i)
            if mat:mats.append(dict(slot=i,path=mat.get_path_name(),vectors=[str(n) for n in unreal.MaterialEditingLibrary.get_vector_parameter_names(mat)],scalars=[str(n) for n in unreal.MaterialEditingLibrary.get_scalar_parameter_names(mat)]))
        rows.append(dict(actor=a.get_name(),cls=a.get_class().get_name(),component=c.get_name(),mesh=m.get_path_name(),origin=[origin.x,origin.y,origin.z],extent=[extent.x,extent.y,extent.z],rotation=str(c.get_world_rotation()),materials=mats))
p=pathlib.Path('D:/HouseCoursework_Team/Saved/Acceptance/HouseholdWeather');p.mkdir(parents=True,exist_ok=True)
(p/'devices.json').write_text(json.dumps(rows,indent=2),encoding='utf8')
unreal.log('HOUSEHOLD_DEVICE_INSPECTION_DONE')
