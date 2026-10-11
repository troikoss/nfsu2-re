#!/usr/bin/env python3
"""Exercise the independent native plugin ABI; optional local-asset integration.

Fixtures publish synthetic state solely to check the adapter protocol. These tests
are not evidence of replacement physics fidelity or recovered byte matching.
"""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def run(command, **kwargs):
    result = subprocess.run([str(x) for x in command], text=True, capture_output=True, **kwargs)
    if result.returncode:
        raise RuntimeError(f"command failed ({result.returncode}): {command}\n{result.stdout}\n{result.stderr}")
    return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--build', type=Path, default=ROOT / 'build/modding')
    parser.add_argument('--game', type=Path, help='Enable integration using a local player installation')
    parser.add_argument('--baseline-build', type=Path, help='Compare an independently mod-disabled build')
    parser.add_argument('--cxx', default='c++')
    args = parser.parse_args()
    build = args.build.resolve()
    work = build / 'mod-tests'
    work.mkdir(parents=True, exist_ok=True)
    fixtures = {'first': 0, 'second': 0, 'custom': 1, 'failinit': 2, 'badabi': 3,
                'badexpert': 4, 'callbackfail': 5, 'late': 6, 'invalid': 7, 'fp': 8,
                'expert': 9, 'createfail': 10, 'syncfail': 11, 'impulsefail': 12}
    for name, kind in fixtures.items():
        directory = work / 'libraries' / name
        directory.mkdir(parents=True, exist_ok=True)
        run([args.cxx, '-std=c++11', '-m32', '-fPIC', '-shared', '-O2', '-fvisibility=hidden',
             '-I', ROOT / 'modding/sdk', f'-DTEST_ID="{name}"', f'-DTEST_KIND={kind}',
             ROOT / 'modding/tests/fixture.cpp', '-o', directory / 'plugin.so'])
    results = []

    def directory_for(name, entries):
        directory = work / name
        directory.mkdir(exist_ok=True)
        for library in fixtures:
            dest = directory / library
            dest.mkdir(exist_ok=True)
            shutil.copy2(work / 'libraries' / library / 'plugin.so', dest / 'plugin.so')
        for example in ('widescreen_hud', 'original_physics'):
            dest = directory / example
            dest.mkdir(exist_ok=True)
            shutil.copy2(build / 'mods' / example / 'plugin.so', dest / 'plugin.so')
        (directory / 'load_order.txt').write_text(entries)
        return directory

    def check(name, entries, mode='default'):
        directory = directory_for(name, entries)
        result = run([build / 'modding/ug2mod_runtime_test', directory, mode])
        results.append(name)
        return result.stdout, result.stderr, directory

    out, err, _ = check('order', '# order\nfirst\nsecond\nfirst\n../escape\nmissing\nbadabi\nbadexpert\nfailinit\n')
    assert out.index('first:init') < out.index('second:init')
    assert 'provider:second' in out and out.count('first:init') == 1
    assert 'badabi:init' not in out and 'badexpert:init' not in out
    assert 'invalid/duplicate' in err and 'missing:' in err
    assert 'failinit:shutdown' in out and 'failinit:event:' not in out
    assert out.index('second:shutdown') < out.index('first:shutdown')
    assert 'custom:init' not in out  # unlisted installed library
    check('custom', 'custom\n', 'custom')
    check('invalid', 'invalid\n', 'invalid')
    check('createfail', 'createfail\n', 'create_failure')
    check('syncfail', 'syncfail\n', 'sync_failure')
    check('impulsefail', 'impulsefail\n', 'impulse_failure')
    out, _, _ = check('callbackfail', 'callbackfail\n')
    assert out.count('callbackfail:event:1\n') == 1 and 'callbackfail:event:10\n' not in out
    out, _, _ = check('late', 'late\n')
    assert out.count('late:late\n') == 1  # not visible during the first event
    check('fp', 'fp\n')
    check('expert', 'expert\n')
    check('hud', 'widescreen_hud\n', 'hud')
    missing = work / 'missing_order'
    missing.mkdir(exist_ok=True)
    out = run([build / 'modding/ug2mod_runtime_test', missing, 'default'])
    assert 'provider:recovered UG2' in out.stdout and 'no load_order.txt' in out.stderr
    results.append('missing_order')
    if args.game:
        game = args.game.resolve()
        custom_dir = directory_for('native_custom', 'custom\n')
        run([build / 'modding/ug2mod_native_test', game, custom_dir])
        results.append('native_custom_collision_and_respawn')
        simulation = [build / 'ug2sim', game, 'SKYLINE', '1200']
        original = run(simulation + ['--no-mods']).stdout
        empty_dir = directory_for('native_empty', '')
        empty = run(simulation + ['--mods-dir', empty_dir]).stdout
        forward_dir = directory_for('native_forwarding', 'original_physics\n')
        forwarded = run(simulation + ['--mods-dir', forward_dir]).stdout
        assert original == empty == forwarded
        if args.baseline_build:
            assert original == run([args.baseline_build.resolve() / 'ug2sim', game, 'SKYLINE', '1200']).stdout
        results.append('1201_frame_baseline_empty_forwarding_equivalence')
        invalid_dir = directory_for('native_invalid', 'invalid\n')
        result = subprocess.run([str(x) for x in simulation + ['--mods-dir', invalid_dir]], text=True, capture_output=True)
        assert result.returncode == 3 and 'invalid state' in result.stderr
        assert 'invalid:destroy' in result.stdout and 'invalid:shutdown' in result.stdout
        results.append('native_invalid_state_cleanup')
    (work / 'results.json').write_text(json.dumps({'passed': results}, indent=2) + '\n')
    print(f"Passed {len(results)} native mod scenarios; report: {work / 'results.json'}")


if __name__ == '__main__':
    main()
