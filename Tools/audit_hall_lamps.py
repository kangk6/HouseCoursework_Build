"""Read-only geometry/material audit of hallway fixtures and authored ON variants."""
import unreal, json, pathlib
out = pathlib.Path('D:/HouseCoursework/Saved/HallLampAudit')
out.mkdir(exist_ok=True)
unreal.EditorLoadingAndSavingUtils.load_map('/Game/HouseFurniture/Maps/Demo_01_Night')
rows = []
names = ['SM_Lamp_E2','SM_Lamp_E3','SM_Lamp_E4','SM_Lamp_C1_2','BP_Lamp_E2_ON_2','BP_Lamp_C2_ON2','SM_Lamp_A1_23','SM_Lamp_B1_2','SM_Lamp_D_6']
for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
    if a.get_name() not in names: continue
    row = {'actor': a.get_name(), 'transform': str(a.get_actor_transform()), 'meshes': [], 'lights': []}
    for light in a.get_components_by_class(unreal.LightComponent):
        row['lights'].append({'name': light.get_name(), 'transform': str(light.get_world_transform()), 'intensity':light.intensity})
    for c in a.get_components_by_class(unreal.StaticMeshComponent):
        entry = {'mesh': c.static_mesh.get_path_name(), 'transform': str(c.get_world_transform()), 'bounds': str(unreal.SystemLibrary.get_component_bounds(c)), 'materials': []}
        for m in c.get_materials():
            chain=[]; src=m
            while isinstance(src,unreal.MaterialInstanceConstant):
                chain.append({'name':src.get_path_name(),'textures':str(src.get_editor_property('texture_parameter_values')),'scalars':str(src.get_editor_property('scalar_parameter_values'))});src=src.get_editor_property('parent')
            chain.append({'base':src.get_path_name(),'emissive':str(unreal.MaterialEditingLibrary.get_material_property_input_node(src,unreal.MaterialProperty.MP_EMISSIVE_COLOR))})
            entry['materials'].append(chain)
        task=unreal.AssetExportTask();task.object=c.static_mesh;task.filename=str(out/(c.static_mesh.get_name()+'.obj'));task.automated=True;task.prompt=False;task.replace_identical=True;task.exporter=unreal.StaticMeshExporterOBJ();unreal.Exporter.run_asset_export_task(task)
        row['meshes'].append(entry)
    rows.append(row)
(out/'audit.json').write_text(json.dumps(rows,indent=2),encoding='utf-8')
