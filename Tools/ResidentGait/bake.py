"""Run with UnrealEditor-Cmd project.uproject -run=pythonscript -script=.../bake.py."""
import pathlib,unreal
tracks=pathlib.Path(__file__).with_name('walk_tracks.json')
result=unreal.CourseworkTools.bake_resident_mocap(str(tracks))
assert result.startswith('SAVED'),result
unreal.log(result)
for suffix in ['Positive90','Negative90']:
 result=unreal.CourseworkTools.bake_resident_gait_clip(str(tracks.with_name('turn_'+suffix+'.json')),'A_ResidentTurn'+suffix)
 assert result.startswith('SAVED'),result
 unreal.log(result)
