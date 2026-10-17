"""Independent C1 shade/bulb emission. Never modify the furniture source material."""
import unreal
lib=unreal.MaterialEditingLibrary
root='/Game/Coursework/Lighting/'
dst=root+'Controlled_MI_Lamp_C1'
if not unreal.EditorAssetLibrary.does_asset_exist(dst):
    src=unreal.load_asset('/Game/HouseFurniture/Materials/MI_Lamp_C1')
    mat=unreal.EditorAssetLibrary.duplicate_asset(src.get_editor_property('parent').get_path_name(),root+'M_HallLampC1')
    pos=lib.create_material_expression(mat,unreal.MaterialExpressionWorldPosition)
    center=lib.create_material_expression(mat,unreal.MaterialExpressionVectorParameter)
    center.set_editor_property('parameter_name','CourseworkFixtureOrigin')
    level=lib.create_material_expression(mat,unreal.MaterialExpressionScalarParameter)
    level.set_editor_property('parameter_name','CourseworkLightLevel');level.set_editor_property('default_value',1.)
    scale=lib.create_material_expression(mat,unreal.MaterialExpressionScalarParameter)
    scale.set_editor_property('parameter_name','CourseworkFixtureScale');scale.set_editor_property('default_value',1.)
    glow=lib.create_material_expression(mat,unreal.MaterialExpressionCustom)
    # C1 has a single atlas including metal, cloth and bulb. Only cloth and
    # bulb emit: the base/stem remain opaque. Model is upright in this map.
    glow.set_editor_property('code','float3 p=(Pos-Origin)/Scale; float shade=step(30.0,p.z)*step(p.z,51.0)*smoothstep(8.0,10.5,length(p.xy)); float bulb=1-smoothstep(3.4,4.2,length(p-float3(0,0,43.5))); return float3(1,.74,.42)*Level*(shade*.65+bulb*5);')
    glow.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    inputs=[]
    for name in ['Pos','Origin','Scale','Level']:
        item=unreal.CustomInput();item.set_editor_property('input_name',name);inputs.append(item)
    glow.set_editor_property('inputs',inputs)
    for node,pin in [(pos,'Pos'),(center,'Origin'),(scale,'Scale'),(level,'Level')]:assert lib.connect_material_expressions(node,'',glow,pin)
    assert lib.connect_material_property(glow,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    lib.recompile_material(mat);unreal.EditorAssetLibrary.save_loaded_asset(mat)
    mi=unreal.EditorAssetLibrary.duplicate_asset(src.get_path_name(),dst)
    lib.set_material_instance_parent(mi,mat);lib.update_material_instance(mi);unreal.EditorAssetLibrary.save_loaded_asset(mi)
unreal.log('HALL_LAMP_MATERIALS_READY')
