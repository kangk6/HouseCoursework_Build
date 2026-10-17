"""Independent source-preserving bulb emission for the two decorative desk lamps."""
import unreal
lib=unreal.MaterialEditingLibrary
root='/Game/Coursework/Lighting/'
if not unreal.EditorAssetLibrary.does_asset_exist(root+'SM_DeskBulb'):
 bulb=unreal.EditorAssetLibrary.duplicate_asset('/Engine/BasicShapes/Sphere',root+'SM_DeskBulb')
 unreal.EditorAssetLibrary.save_loaded_asset(bulb)
if not unreal.EditorAssetLibrary.does_asset_exist(root+'M_DeskBulb'):
 factory=unreal.MaterialFactoryNew()
 m=unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_DeskBulb',root.rstrip('/'),unreal.Material,factory)
 m.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
 level=lib.create_material_expression(m,unreal.MaterialExpressionScalarParameter)
 level.set_editor_property('parameter_name','CourseworkLightLevel');level.set_editor_property('default_value',1.0)
 color=lib.create_material_expression(m,unreal.MaterialExpressionConstant3Vector)
 color.set_editor_property('constant',unreal.LinearColor(5,3.7,2.1,1))
 mul=lib.create_material_expression(m,unreal.MaterialExpressionMultiply)
 lib.connect_material_expressions(level,'',mul,'A');lib.connect_material_expressions(color,'',mul,'B')
 lib.connect_material_property(mul,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
 lib.recompile_material(m);unreal.EditorAssetLibrary.save_loaded_asset(m)
for name in ['MI_TableLamp_A','MI_TableLamp_A1']:
 src=unreal.load_asset('/Game/HouseFurniture/Materials/'+name)
 base=src.get_editor_property('parent')
 root='/Game/Coursework/Lighting/'
 dst=root+'M_Bulb_'+name
 # Existing generated materials are already complete; do not append nodes on rerun.
 if unreal.EditorAssetLibrary.does_asset_exist(root+'Controlled_'+name):continue
 mat=unreal.EditorAssetLibrary.duplicate_asset(base.get_path_name(),dst)
 # Keep all the furniture's texture, normal, roughness and metallic connections.
 pos=lib.create_material_expression(mat,unreal.MaterialExpressionWorldPosition)
 center=lib.create_material_expression(mat,unreal.MaterialExpressionVectorParameter)
 center.set_editor_property('parameter_name','CourseworkBulbCenter')
 radius=lib.create_material_expression(mat,unreal.MaterialExpressionScalarParameter)
 radius.set_editor_property('parameter_name','CourseworkBulbRadius');radius.set_editor_property('default_value',2.5)
 level=lib.create_material_expression(mat,unreal.MaterialExpressionScalarParameter)
 level.set_editor_property('parameter_name','CourseworkLightLevel');level.set_editor_property('default_value',1.0)
 glow=lib.create_material_expression(mat,unreal.MaterialExpressionCustom)
 glow.set_editor_property('code','return float3(1.0,0.74,0.42)*5.0*Level*(1-smoothstep(Radius-0.35,Radius,length(Pos-Center)));')
 glow.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT3)
 inputs=[]
 for x in ['Pos','Center','Radius','Level']:
  value=unreal.CustomInput();value.set_editor_property('input_name',x);inputs.append(value)
 glow.set_editor_property('inputs',inputs)
 for node,pin in [(pos,'Pos'),(center,'Center'),(radius,'Radius'),(level,'Level')]:assert lib.connect_material_expressions(node,'',glow,pin)
 assert lib.connect_material_property(glow,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
 lib.recompile_material(mat);unreal.EditorAssetLibrary.save_loaded_asset(mat)
 path=root+'Controlled_'+name
 mi=unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else unreal.EditorAssetLibrary.duplicate_asset(src.get_path_name(),path)
 lib.set_material_instance_parent(mi,mat);lib.update_material_instance(mi);unreal.EditorAssetLibrary.save_loaded_asset(mi)
unreal.log('TABLE_LAMP_MATERIALS_READY')
