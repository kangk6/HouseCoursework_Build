import unreal,json,pathlib
unreal.EditorLoadingAndSavingUtils.load_map('/Game/HouseFurniture/Maps/Demo_01_Night')
rows=[]
for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
 for c in a.get_components_by_class(unreal.StaticMeshComponent):
  if not c.static_mesh:continue
  p,e,r=unreal.SystemLibrary.get_component_bounds(c)
  if not (-2700<p.x<-1400 and 8900<p.y<11000 and -20<p.z<750):continue
  rows.append(dict(actor=a.get_name(),mesh=c.static_mesh.get_path_name(),center=list(p.to_tuple()),extent=list(e.to_tuple()),location=list(c.get_world_transform().translation.to_tuple()),rotation=list(c.get_world_transform().rotation.rotator().to_tuple()),scale=list(c.get_world_transform().scale3d.to_tuple()),materials=[c.get_material(i).get_path_name() if c.get_material(i) else '' for i in range(c.get_num_materials())]))
pathlib.Path('D:/HouseCoursework/Saved/VentilationAudit/full_upper.json').write_text(json.dumps(rows,ensure_ascii=False,indent=2),encoding='utf-8')
unreal.log(unreal.CourseworkTools.export_window_geometry('D:/HouseCoursework/Saved/VentilationAudit/geometry.json'));unreal.log('UPPER_AUDIT_DONE')
