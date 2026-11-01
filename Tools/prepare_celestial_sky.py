"""Add celestial bodies to a copy of the authored sky, reusing its cloud mask."""
from pathlib import Path
import unreal
lib=unreal.MaterialEditingLibrary
folder='/Game/Coursework/Celestial';path=folder+'/M_CelestialSky'
unreal.EditorAssetLibrary.make_directory(folder)
m=unreal.load_asset(path)
if m is None:m=unreal.EditorAssetLibrary.duplicate_asset('/Engine/EngineSky/M_Sky_Panning_Clouds2',path)
# Rebuild only this generated copy on repeat; never edit Engine or house assets.
else:
 unreal.EditorAssetLibrary.delete_asset(path)
 m=unreal.EditorAssetLibrary.duplicate_asset('/Engine/EngineSky/M_Sky_Panning_Clouds2',path)
original=lib.get_material_property_input_node(m,unreal.MaterialProperty.MP_EMISSIVE_COLOR)
nodes={}
def collect(n):
 if not n or n.get_name() in nodes:return
 nodes[n.get_name()]=n
 for x in lib.get_inputs_for_material_expression(m,n):collect(x)
collect(original)
cloud=nodes['MaterialExpressionClamp_12']
assert cloud.get_class().get_name()=='MaterialExpressionClamp'
shape=lib.create_material_expression(m,unreal.MaterialExpressionCustom)
shape.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT3)
names=['View','SunDir','MoonDir','SunColor','SunAmount','MoonAmount','StarAmount','TwilightAmount','Spin','CloudMask','Time']
inputs=[]
for name in names:
 i=unreal.CustomInput();i.set_editor_property('input_name',name);inputs.append(i)
shape.set_editor_property('inputs',inputs)
shape.set_editor_property('code',(Path(__file__).parent/'Data/celestial.hlsl').read_text(encoding='utf-8'))
view=lib.create_material_expression(m,unreal.MaterialExpressionCameraVectorWS)
lib.connect_material_expressions(view,'',shape,'View')
lib.connect_material_expressions(cloud,'',shape,'CloudMask')
time=lib.create_material_expression(m,unreal.MaterialExpressionTime)
lib.connect_material_expressions(time,'',shape,'Time')
for name in ['SunDir','MoonDir','SunColor']:
 n=lib.create_material_expression(m,unreal.MaterialExpressionVectorParameter)
 n.set_editor_property('parameter_name','Celestial'+name)
 n.set_editor_property('default_value',unreal.LinearColor(0,0,1,1))
 lib.connect_material_expressions(n,'',shape,name)
for name in ['SunAmount','MoonAmount','StarAmount','TwilightAmount','Spin']:
 n=lib.create_material_expression(m,unreal.MaterialExpressionScalarParameter)
 n.set_editor_property('parameter_name','Celestial'+name);n.set_editor_property('default_value',0.)
 lib.connect_material_expressions(n,'',shape,name)
add=lib.create_material_expression(m,unreal.MaterialExpressionAdd)
lib.connect_material_expressions(original,'',add,'A');lib.connect_material_expressions(shape,'',add,'B')
lib.connect_material_property(add,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
lib.recompile_material(m);unreal.EditorAssetLibrary.save_loaded_asset(m)
unreal.log('CELESTIAL_SKY_READY')
