import unreal,json,pathlib
root='/Game/Coursework/Lighting'
unreal.EditorAssetLibrary.make_directory(root)
parents={};report=[]
for path in ['/Game/HouseFurniture/Materials/Master/M_Lamp_SSS','/Game/HouseFurniture/Materials/Master/M_Lamp_Illum1','/Game/HouseFurniture/Materials/Generic/M_Plastic_Illum']:
 src=unreal.load_asset(path);dst=root+'/Controlled_'+src.get_name()
 if unreal.EditorAssetLibrary.does_asset_exist(dst):
  parents[src.get_path_name()]=unreal.load_asset(dst);continue
 mat=unreal.EditorAssetLibrary.duplicate_asset(path,dst)
 lib=unreal.MaterialEditingLibrary
 original=lib.get_material_property_input_node(mat,unreal.MaterialProperty.MP_EMISSIVE_COLOR)
 output=lib.get_material_property_input_node_output_name(mat,unreal.MaterialProperty.MP_EMISSIVE_COLOR)
 scalar=lib.create_material_expression(mat,unreal.MaterialExpressionScalarParameter,700,400)
 scalar.set_editor_property('parameter_name','CourseworkLightLevel');scalar.set_editor_property('default_value',1.0)
 mul=lib.create_material_expression(mat,unreal.MaterialExpressionMultiply,900,400)
 assert lib.connect_material_expressions(original,output,mul,'A')
 assert lib.connect_material_expressions(scalar,'',mul,'B')
 assert lib.connect_material_property(mul,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
 lib.recompile_material(mat);unreal.EditorAssetLibrary.save_loaded_asset(mat)
 parents[src.get_path_name()]=mat
unreal.EditorLoadingAndSavingUtils.load_map('/Game/HouseFurniture/Maps/Demo_01_Night')
for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
 if a.get_name() not in ['SM_Lamp_A1_23','BP_Lamp_C2_ON2','BP_Lamp_C2_ON_2','SM_Lamp_D_6']:continue
 for c in a.get_components_by_class(unreal.StaticMeshComponent):
  for mat in c.get_materials():
   if not isinstance(mat,unreal.MaterialInstanceConstant):continue
   parent=mat.get_editor_property('parent')
   if parent.get_path_name() not in parents:continue
   dst=root+'/Controlled_'+mat.get_name()
   if unreal.EditorAssetLibrary.does_asset_exist(dst):continue
   copied=unreal.EditorAssetLibrary.duplicate_asset(mat.get_path_name(),dst)
   unreal.MaterialEditingLibrary.set_material_instance_parent(copied,parents[parent.get_path_name()])
   unreal.MaterialEditingLibrary.update_material_instance(copied)
   unreal.EditorAssetLibrary.save_loaded_asset(copied)
   report.append({'original':mat.get_path_name(),'controlled':copied.get_path_name()})
pathlib.Path('D:/HouseCoursework/Saved/ControlledMaterials.json').write_text(json.dumps(report,indent=2))
