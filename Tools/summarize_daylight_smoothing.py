from pathlib import Path
import json,sys
root=Path(sys.argv[1]) if len(sys.argv)>1 else Path(__file__).resolve().parents[1]/'Docs/DaylightSmoothing/Final'
results=[]
for height in [720,1080]:
 r=json.loads((root/f'sweep_{height}.json').read_text(encoding='utf-8-sig'))
 a=r['samples'];assert r['realtime_sky'] and len(a)>=200
 assert max(x['luma'] for x in a)>5,'Invalid or black image readback'
 per=[]
 for sc in range(3):
  b=[x for x in a if x['scenario']==sc and x['stage'] in [1,2]]
  hold=[x for x in b if x['stage']==2 and x['elapsed']>.75]
  assert len(b)>20 and len(hold)>5
  max_jump=max(abs(y['sky']-x['sky']) for x,y in zip(b,b[1:]))
  sky_rate=max(abs(y['sky']-x['sky'])/max(y['dt'],.001) for x,y in zip(b,b[1:]))
  hour_rate=max(abs(y['rendered_hour']-x['rendered_hour'])/max(y['dt'],.001) for x,y in zip(b,b[1:]))
  late_step=max(abs(y['luma']-x['luma']) for x,y in zip(hold,hold[1:]))
  error=abs(hold[-1]['rendered_hour']-hold[-1]['target'])
  assert sky_rate<12.5 and hour_rate<12.5 and late_step<.5 and error<.002,(height,sc,sky_rate,hour_rate,late_step,error)
  per.append({'scenario':sc,'samples':len(b),'max_sky_step':max_jump,'max_sky_change_per_second':sky_rate,'max_hour_change_per_second':hour_rate,'late_hold_luma_step_0_to_255':late_step,'final_hour_error':error,'passed':True})
 results.append({'height':height,'samples':len(a),'cases':per,'passed':True})
summary={'passed':True,'runs':results,'thresholds':{'max_sky_change_per_second':12.5,'max_hour_change_per_second':12.5,'late_hold_luma_step':.5,'final_hour_error':.002},'baseline':{'source_update_behavior':'v0.15.0-daylight with read-only telemetry and synthetic drag harness','sky_step':.82,'luma_excluded':True,'reason':'Baseline viewport ReadPixels returned black; baseline is used only for light parameter discontinuity, not pixel or FPS comparisons'},'method':'Actual Slate drag handlers and every-frame Slate window backbuffer readback; right scene region excludes controls; screenshot sampling overhead is not a normal FPS benchmark','visual_reviewed':False}
(root/'smoothing_summary.json').write_text(json.dumps(summary,ensure_ascii=False,indent=2),encoding='utf-8')
print('Continuous drag/hold checks passed at both resolutions')
