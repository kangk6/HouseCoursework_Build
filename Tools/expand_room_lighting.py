"""Prepare isolated emissive material copies for every authored light fixture."""
import json, pathlib, unreal
root='/Game/Coursework/Lighting'
lib=unreal.MaterialEditingLibrary
cache={}; report=[]
unreal.EditorAssetLibrary.make_directory(root)

def controlled(src):
    key=src.get_path_name()
    if key in cache: return cache[key]
    dst=root+'/Controlled_'+src.get_name()
    if unreal.EditorAssetLibrary.does_asset_exist(dst):
        cache[key]=unreal.load_asset(dst);return cache[key]
    if isinstance(src,unreal.MaterialInstanceConstant):
        parent=controlled(src.get_editor_property('parent'))
        if not parent: cache[key]=None;return None
        result=unreal.EditorAssetLibrary.duplicate_asset(key,dst)
        lib.set_material_instance_parent(result,parent)
        lib.update_material_instance(result)
    elif isinstance(src,unreal.Material):
        original=lib.get_material_property_input_node(src,unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        if not original: cache[key]=None;return None
        result=unreal.EditorAssetLibrary.duplicate_asset(key,dst)
        original=lib.get_material_property_input_node(result,unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        output=lib.get_material_property_input_node_output_name(result,unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        scalar=lib.create_material_expression(result,unreal.MaterialExpressionScalarParameter,700,400)
        scalar.set_editor_property('parameter_name','CourseworkLightLevel')
        scalar.set_editor_property('default_value',1.0)
        mul=lib.create_material_expression(result,unreal.MaterialExpressionMultiply,900,400)
        assert lib.connect_material_expressions(original,output,mul,'A')
        assert lib.connect_material_expressions(scalar,'',mul,'B')
        assert lib.connect_material_property(mul,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        lib.recompile_material(result)
    else: cache[key]=None;return None
    unreal.EditorAssetLibrary.save_loaded_asset(result)
    cache[key]=result
    return result

unreal.EditorLoadingAndSavingUtils.load_map('/Game/HouseFurniture/Maps/Demo_01_Night')
for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
    if not any(word in actor.get_name() for word in ('Lamp','Mirror')):continue
    mats=[]
    for mesh in actor.get_components_by_class(unreal.StaticMeshComponent):
        for src in mesh.get_materials():
            if not src:continue
            dst=controlled(src)
            mats.append({'source':src.get_path_name(),'controlled':dst.get_path_name() if dst else None})
    p=actor.get_actor_location()
    report.append({'actor':actor.get_name(),'position':[p.x,p.y,p.z],'materials':mats})
out=pathlib.Path('D:/HouseCoursework/Docs/WholeHouseLighting')
out.mkdir(parents=True,exist_ok=True)
(out/'fixture_materials.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
