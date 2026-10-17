import unreal,runpy,pathlib
root=pathlib.Path('D:/HouseCoursework');out=root/'Saved/VentilationAudit';out.mkdir(parents=True,exist_ok=True)
unreal.EditorLoadingAndSavingUtils.load_map('/Game/HouseFurniture/Maps/Demo_01_Night')
r=unreal.CourseworkTools.export_window_geometry(str(out/'geometry.json'));unreal.log(r);assert r.startswith('SAVED'),r
runpy.run_path(str(root/'Tools/prepare_window_mechanics.py'),run_name='__main__')
runpy.run_path(str(root/'Tools/prepare_curtain_wind.py'),run_name='__main__')
