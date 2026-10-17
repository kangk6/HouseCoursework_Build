import unreal,json,pathlib
system=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
out=[]
for path in ['/Game/HouseFurniture/Maps/Demo_01_Night','/Game/HouseFurniture/Maps/Demo_01']:
    world=unreal.EditorLoadingAndSavingUtils.load_map(path)
    resident=[a for a in system.get_all_level_actors() if a.get_actor_label().startswith('Coursework_Resident')]
    traces=[]
    for x,y in [(-2310,9940),(-2300,9940),(-2320,9940)]:
        hit=unreal.SystemLibrary.line_trace_single(world,unreal.Vector(x,y,90),unreal.Vector(x,y,0),unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,True,resident,unreal.DrawDebugTrace.NONE,True)
        traces.append({'x':x,'y':y,'hit':str(hit),'point':str(hit.get_editor_property('impact_point')) if hit else None})
    nearby=[]
    for a in system.get_all_level_actors():
        for c in a.get_components_by_class(unreal.StaticMeshComponent):
            p,e,r=unreal.SystemLibrary.get_component_bounds(c)
            if -2480<p.x<-1950 and 9700<p.y<10200 and 50<p.z<270:
                nearby.append({'actor':a.get_actor_label(),'component':c.get_name(),'center':[p.x,p.y,p.z],'extent':[e.x,e.y,e.z]})
    out.append({'map':path,'ground':traces,'nearby':nearby})
pathlib.Path('D:/House_assets/ResidentRefined/ground.json').write_text(json.dumps(out,indent=2),encoding='utf8')
