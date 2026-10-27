"""Create additive rain materials. Never edits original road/house materials."""
import unreal

lib=unreal.MaterialEditingLibrary
folder='/Game/Coursework/RainPolish'
unreal.EditorAssetLibrary.make_directory(folder)
tools=unreal.AssetToolsHelpers.get_asset_tools()

def fresh(name):
    path=folder+'/'+name
    m=unreal.load_asset(path)
    if m is None:m=tools.create_asset(name,folder,unreal.Material,unreal.MaterialFactoryNew())
    lib.delete_all_material_expressions(m)
    return m

def scalar(m,name,value):
    n=lib.create_material_expression(m,unreal.MaterialExpressionScalarParameter)
    n.set_editor_property('parameter_name',name);n.set_editor_property('default_value',value)
    return n

def inputs(*names):
    result=[]
    for name in names:
        value=unreal.CustomInput();value.set_editor_property('input_name',name);result.append(value)
    return result

for name,splash in [('M_RainStreak',False),('M_RainSplash',True)]:
    if unreal.EditorAssetLibrary.does_asset_exist(folder+'/'+name):continue
    m=fresh(name)
    m.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT)
    m.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property('two_sided',True)
    m.set_editor_property('used_with_instanced_static_meshes',True)
    uv=lib.create_material_expression(m,unreal.MaterialExpressionTextureCoordinate)
    opacity=scalar(m,'RainOpacity',.30 if not splash else .24)
    shape=lib.create_material_expression(m,unreal.MaterialExpressionCustom)
    shape.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    shape.set_editor_property('inputs',inputs('UV','Opacity'))
    shape.set_editor_property('code',
        'float2 p=UV*2-1; return saturate(1-dot(p,p))*Opacity;'
        if splash else 'float x=saturate(1-abs(UV.x*2-1)); float y=saturate(sin(UV.y*3.14159265)); return pow(x,1.5)*sqrt(y)*Opacity;')
    lib.connect_material_expressions(uv,'',shape,'UV');lib.connect_material_expressions(opacity,'',shape,'Opacity')
    lib.connect_material_property(shape,'',unreal.MaterialProperty.MP_OPACITY)
    tint=lib.create_material_expression(m,unreal.MaterialExpressionVectorParameter)
    tint.set_editor_property('parameter_name','RainTint');tint.set_editor_property('default_value',unreal.LinearColor(.32,.37,.42,1))
    lib.connect_material_property(tint,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    lib.recompile_material(m);unreal.EditorAssetLibrary.save_loaded_asset(m)

path=folder+'/M_RoadWet'
if not unreal.EditorAssetLibrary.does_asset_exist(path):
    m=unreal.EditorAssetLibrary.duplicate_asset('/Game/HouseFurniture/Materials/Master/M_Road_A',path)
    wet=scalar(m,'CourseworkWetness',0)
    for prop,dry,wet_value in [(unreal.MaterialProperty.MP_BASE_COLOR,1.,.72),(unreal.MaterialProperty.MP_ROUGHNESS,1.,.38)]:
        original=lib.get_material_property_input_node(m,prop)
        output=lib.get_material_property_input_node_output_name(m,prop)
        assert original is not None,prop
        amount=lib.create_material_expression(m,unreal.MaterialExpressionCustom)
        amount.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT1)
        amount.set_editor_property('inputs',inputs('Wet'))
        amount.set_editor_property('code',f'return lerp({dry},{wet_value},saturate(Wet));')
        lib.connect_material_expressions(wet,'',amount,'Wet')
        mul=lib.create_material_expression(m,unreal.MaterialExpressionMultiply)
        lib.connect_material_expressions(original,output,mul,'A');lib.connect_material_expressions(amount,'',mul,'B')
        lib.connect_material_property(mul,'',prop)
    lib.recompile_material(m);unreal.EditorAssetLibrary.save_loaded_asset(m)
unreal.log('RAIN_POLISH_MATERIALS_READY')
