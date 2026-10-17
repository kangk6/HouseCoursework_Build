"""Run with UE PythonScript commandlet; preserve the original StarterContent."""
import unreal
folder = '/Game/Coursework/Safety'
unreal.EditorAssetLibrary.make_directory(folder)
material = unreal.load_asset(folder + '/M_KitchenSmoke')
if material is None:
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_KitchenSmoke', folder, unreal.Material, unreal.MaterialFactoryNew())
material.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
material.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
material.set_editor_property('two_sided', True)
material.set_editor_property('used_with_particle_sprites', True)
edit = unreal.MaterialEditingLibrary
edit.delete_all_material_expressions(material)
texture = edit.create_material_expression(material, unreal.MaterialExpressionParticleSubUV, -650, 100)
texture.set_editor_property('texture', unreal.load_asset('/Game/StarterContent/Textures/T_Smoke_SubUV'))
particle = edit.create_material_expression(material, unreal.MaterialExpressionParticleColor, -650, 300)
opacity = edit.create_material_expression(material, unreal.MaterialExpressionMultiply, -400, 100)
# The atlas carries the soft smoke silhouette in alpha, not its white RGB.
assert edit.connect_material_expressions(texture, 'A', opacity, 'A')
assert edit.connect_material_expressions(particle, 'A', opacity, 'B')
gain = edit.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -400, 300)
gain.set_editor_property('parameter_name', 'SmokeOpacity')
gain.set_editor_property('default_value', 0.4)
scaled = edit.create_material_expression(material, unreal.MaterialExpressionMultiply, -180, 100)
assert edit.connect_material_expressions(opacity, '', scaled, 'A')
assert edit.connect_material_expressions(gain, '', scaled, 'B')
fade = edit.create_material_expression(material, unreal.MaterialExpressionDepthFade, 50, 100)
fade.set_editor_property('fade_distance_default', 18.0)
assert edit.connect_material_expressions(scaled, '', fade, 'Opacity')
assert edit.connect_material_property(fade, '', unreal.MaterialProperty.MP_OPACITY)
grey = edit.create_material_expression(material, unreal.MaterialExpressionConstant3Vector, -180, -150)
grey.set_editor_property('constant', unreal.LinearColor(0.04, 0.045, 0.05, 1.0))
assert edit.connect_material_property(grey, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
edit.recompile_material(material)
unreal.EditorAssetLibrary.save_loaded_asset(material)
result = unreal.CourseworkTools.prepare_kitchen_smoke()
unreal.log('KITCHEN_SMOKE_PREP ' + result)
if result.startswith('ERROR'):
    raise RuntimeError(result)
fire = unreal.CourseworkTools.prepare_kitchen_fire()
assert fire.startswith('SAVED'), fire
unreal.log(fire)

