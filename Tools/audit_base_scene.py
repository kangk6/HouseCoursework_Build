import unreal,json,pathlib,collections
root=pathlib.Path(__file__).resolve().parents[1]
world=unreal.EditorLoadingAndSavingUtils.load_map('/Game/HouseFurniture/Maps/Demo_01_Night')
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
counts=collections.Counter(); empty=[]; missing_materials=[]; meshes=set(); humans=[]
categories={key:0 for key in ['chair','table','sofa','bed','cabinet','fridge','sink','toilet','lamp']}
for actor in actors:
    if 'CourseworkResidentIdle' in [str(tag) for tag in actor.tags]:
        c=actor.get_component_by_class(unreal.SkeletalMeshComponent)
        humans.append({'actor':actor.get_actor_label(),'mesh':c.get_skeletal_mesh_asset().get_path_name(),'bones':c.get_num_bones(),'animation_data':str(c.get_editor_property('animation_data'))})
    for c in actor.get_components_by_class(unreal.StaticMeshComponent):
        counts['static_mesh_components']+=1
        mesh=c.static_mesh
        if not mesh:
            empty.append({'actor':actor.get_actor_label(),'component':c.get_name()});continue
        meshes.add(mesh.get_path_name())
        for index in range(c.get_num_materials()):
            if not c.get_material(index): missing_materials.append({'actor':actor.get_actor_label(),'component':c.get_name(),'slot':index})
        p,e,r=unreal.SystemLibrary.get_component_bounds(c)
        if -2600<p.x<-1500 and 9000<p.y<10750 and -20<p.z<1000:
            name=mesh.get_name().lower()
            for key in categories:
                if key in name: categories[key]+=1
output=root/'Docs/BaseAcceptance'
output.mkdir(parents=True,exist_ok=True)
report={'map':world.get_path_name(),'actor_count':len(actors),'static_mesh_component_count':counts['static_mesh_components'],'unique_static_meshes':len(meshes),'empty_mesh_components':empty,'missing_material_slots':missing_materials,'main_house_object_categories':categories,'humans':humans,'scope':'Saved source map inspection, not a substitute for rendering/UV/clipping checks'}
(output/'scene_audit.json').write_text(json.dumps(report,indent=2,ensure_ascii=False),encoding='utf8')
unreal.log('BASE_SCENE_AUDIT_COMPLETE')
