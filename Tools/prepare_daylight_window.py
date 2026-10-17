import unreal,json,pathlib
unreal.load_module('StaticMeshEditor')
sys=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
sources=['/Game/HouseFurniture/Meshes/House/Wall/Windows/SM_Window_Floor1_A3_400','/Game/HouseFurniture/Meshes/SM_Curtains_A1','/Game/HouseFurniture/Meshes/SM_Curtains_A2','/Game/HouseFurniture/Meshes/SM_Curtains_A3','/Game/HouseFurniture/Meshes/SM_Table_A3/SM_Table_A3']
unreal.EditorAssetLibrary.make_directory('/Game/Coursework/Daylight')
out=[]
for src in sources:
 source=unreal.load_asset(src)
 changes=[]
 for lod in range(source.get_num_lods()):
  for section in range(source.get_num_sections(lod)):
   slot=sys.get_lod_material_slot(source,lod,section);mat=source.get_material(slot)
   if mat and mat.get_base_material().get_editor_property('blend_mode')==unreal.BlendMode.BLEND_TRANSLUCENT:changes.append({'lod':lod,'section':section,'slot':slot,'material':mat.get_path_name()})
 if not changes:continue
 dst='/Game/Coursework/Daylight/'+source.get_name()+'_Daylight'
 mesh=unreal.EditorAssetLibrary.duplicate_asset(src,dst) if not unreal.EditorAssetLibrary.does_asset_exist(dst) else unreal.load_asset(dst)
 for c in changes:sys.enable_section_cast_shadow(mesh,False,c['lod'],c['section'])
 unreal.EditorAssetLibrary.save_loaded_asset(mesh)
 out.append({'source':src,'duplicate':dst,'transparent_sections_no_opaque_shadow':changes})
assert out,'No translucent sections found'
pathlib.Path('D:/HouseCoursework/Saved/DaylightWindowPrepared.json').write_text(json.dumps(out,indent=2),encoding='utf-8')
