"""Run with UnrealEditor-Cmd project.uproject -run=pythonscript -script=.../bake.py."""
import pathlib,unreal
tracks=pathlib.Path(__file__).with_name('walk_tracks.json')
result=unreal.CourseworkTools.bake_resident_mocap(str(tracks))
assert result.startswith('SAVED'),result
unreal.log(result)
