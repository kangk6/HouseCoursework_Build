import unreal, json, pathlib
out=pathlib.Path('D:/HouseCoursework/Saved/LampBugAudit');out.mkdir(exist_ok=True)
unreal.EditorLoadingAndSavingUtils.load_map('/Game/HouseFurniture/Maps/Demo_01_Night')
rows=[]
for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
 if a.get_name() not in ['SM_TableLamp_A_26','SM_TableLamp_A1_32']:continue
 for c in a.get_components_by_class(unreal.StaticMeshComponent):
  row={'actor':a.get_name(),'bounds':str(unreal.SystemLibrary.get_component_bounds(c)),'transform':str(c.get_world_transform()),'materials':[]}
  for m in c.get_materials():
   chain=[];src=m
   while isinstance(src,unreal.MaterialInstanceConstant):
    chain.append({'name':src.get_path_name(),'textures':str(src.get_editor_property('texture_parameter_values')),'scalars':str(src.get_editor_property('scalar_parameter_values'))});src=src.get_editor_property('parent')
   chain.append({'base':src.get_path_name(),'emissive':str(unreal.MaterialEditingLibrary.get_material_property_input_node(src,unreal.MaterialProperty.MP_EMISSIVE_COLOR))})
   row['materials'].append(chain)
  task=unreal.AssetExportTask();task.object=c.static_mesh;task.filename=str(out/(c.static_mesh.get_name()+'.obj'));task.automated=True;task.prompt=False;task.replace_identical=True;task.exporter=unreal.StaticMeshExporterOBJ();unreal.Exporter.run_asset_export_task(task)
  rows.append(row)
(out/'audit.json').write_text(json.dumps(rows,indent=2),encoding='utf-8')
