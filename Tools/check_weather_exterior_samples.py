"""Check fixed exterior readbacks; images still require visual review."""
import argparse
import json
from pathlib import Path


def summarize(report):
    samples = report['samples']
    assert len(samples) == 24
    assert {(s['view'], s['scene']) for s in samples} == {(v, s) for v in range(6) for s in range(4)}
    cloudy = [s for s in samples if s['scene'] in (1, 2)]
    checks = [{'case': 'cloudy_view_%d_scene_%d_not_clipped' % (s['view'], s['scene']),
               'passed': s['sky_clipped'] < .01} for s in cloudy]
    checks += [{'case': 'rain_noon_view_%d_ground_readable' % s['view'],
                'passed': 35 < s['ground_luma'] < 200}
               for s in samples if s['scene'] == 2]
    return {'checks': checks,
            'maximum_cloudy_clipped_fraction': max(s['sky_clipped'] for s in cloudy),
            'scope': 'Six fixed native-camera views, clear/overcast/rain noon and rain midnight. Upper ROI includes trees and buildings; this is not a sky segmentation or all-location certification.'}


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('report', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    result = summarize(json.loads(args.report.read_text(encoding='utf-8-sig')))
    args.output.write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print('%d exterior image checks; max cloudy clipping %.5f' %
          (len(result['checks']), result['maximum_cloudy_clipped_fraction']))
    raise SystemExit(0 if all(c['passed'] for c in result['checks']) else 1)
