import unreal,json,pathlib
out=pathlib.Path('D:/HouseCoursework/Saved/VentilationAudit');out.mkdir(parents=True,exist_ok=True)
unreal.EditorLoadingAndSavingUtils.load_map('/Game/HouseFurniture/Maps/Demo_01_Night')
rows=[];meshes={}
for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
 for c in a.get_components_by_class(unreal.StaticMeshComponent):
  m=c.static_mesh
  if not m:continue
  p,e,r=unreal.SystemLibrary.get_component_bounds(c)
  if not(-2600<p.x<-1500 and 8900<p.y<10800 and -20<p.z<850):continue
  if not any(s in m.get_name().lower() for s in ['window','curtain','plant','flower','door','shutter']):continue
  t=c.get_world_transform()
  rows.append(dict(actor=a.get_name(),label=a.get_actor_label(),component=c.get_name(),mesh=m.get_path_name(),center=[p.x,p.y,p.z],extent=[e.x,e.y,e.z],location=list(t.translation.to_tuple()),rotation=list(t.rotation.rotator().to_tuple()),scale=list(t.scale3d.to_tuple()),movable=str(c.mobility),materials=[c.get_material(i).get_path_name() if c.get_material(i) else '' for i in range(c.get_num_materials())]))
  if 'window' in m.get_name().lower():meshes[m.get_name()]=m
(out/'inventory.json').write_text(json.dumps(rows,ensure_ascii=False,indent=2),encoding='utf-8')
for name,m in meshes.items():
 task=unreal.AssetExportTask();task.object=m;task.filename=str(out/(name+'.obj'));task.automated=True;task.prompt=False;task.replace_identical=True;task.exporter=unreal.StaticMeshExporterOBJ()
 unreal.Exporter.run_asset_export_task(task)
unreal.log('VENTILATION_AUDIT_DONE')
