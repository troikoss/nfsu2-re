#!/usr/bin/env python3
"""Capture local-asset HUD regression images; compare 4:3 and anchoring masks."""
import argparse
import json
from pathlib import Path
import subprocess
from PIL import Image, ImageChops

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--build', type=Path, default=ROOT / 'build/modding')
    parser.add_argument('--game', type=Path, default=ROOT / 'Need for Speed Underground 2')
    parser.add_argument('--skins', default=','.join(str(i) for i in range(11)))
    args = parser.parse_args()
    build = args.build.resolve()
    output = build / 'mod-tests/hud'
    output.mkdir(parents=True, exist_ok=True)
    reports = []

    def capture(width, height, mode, skin=0, frames=1):
        name = f'{width}x{height}-skin{skin}-{mode}' + (f'-frame{frames}' if frames != 1 else '')
        path = output / (name + '.ppm')
        command = [str(build / 'ug2demo'), '--game', str(args.game.resolve()), '--frames', str(frames),
                   '--width', str(width), '--height', str(height), '--hud-skin', str(skin), '--screenshot', str(path)]
        if frames > 1:
            command += ['--scripted']
        if mode == 'baseline':
            command += ['--no-mods']
        elif mode == 'scene':
            command += ['--no-mods', '--no-hud']
        elif mode == 'scene-modded':
            command += ['--no-hud']
        result = subprocess.run(command, text=True, capture_output=True)
        (output / (name + '.log')).write_text(result.stdout + result.stderr)
        if result.returncode:
            raise RuntimeError(f'{name}: return {result.returncode}: {result.stderr}')
        image = Image.open(path).convert('RGB')
        if image.size != (width, height):
            raise AssertionError(f'framebuffer size differs: {image.size}')
        image.save(output / (name + '.png'))
        return image

    centre_reference = None
    for width, height in [(640, 480), (1280, 720), (1680, 720), (2560, 720)]:
        baseline = capture(width, height, 'baseline')
        modded = capture(width, height, 'modded')
        if width * 3 == height * 4:
            assert ImageChops.difference(baseline, modded).getbbox() is None
        scene = capture(width, height, 'scene')
        scene_modded = capture(width, height, 'scene-modded')
        if height == 720:
            centre = scene_modded.crop((width // 2 - 320, 0, width // 2 + 320, height))
            if centre_reference is None:
                centre_reference = centre
            else:
                difference = ImageChops.difference(centre, centre_reference).convert('L')
                changed = sum(difference.histogram()[3:])
                assert changed < 640 * 720 * 0.005, ('FOV central view changed', width, changed)
        # Differences from the same no-HUD scene isolate overlay coverage.
        masks = []
        for image, background in ((baseline, scene), (modded, scene_modded)):
            difference = ImageChops.difference(image, background).convert('L').point(lambda p: 255 if p > 3 else 0)
            masks.append(difference)
        shift = max(0, (width - height * 4 / 3) / 2)
        bounds = []
        for side in ('left', 'right'):
            region = (0, int(height * .6), width // 2, height) if side == 'left' else (width // 2, int(height * .6), width, height)
            old = masks[0].crop(region).getbbox()
            new = masks[1].crop(region).getbbox()
            assert old and new, (side, width, old, new)
            expected = -shift if side == 'left' else shift
            assert abs((new[0] - old[0]) - expected) <= 2, (side, width, old, new, expected)
            # Translucent outer pixels blend against different scenery after moving.
            # The opaque leading edge gives a tighter translation check; allow a
            # bounded threshold difference at the faint trailing edge.
            assert abs((new[2] - old[2]) - expected) <= 8, (side, width, old, new, expected)
            bounds.append({'side': side, 'before': old, 'after': new, 'shift_px': expected})
        reports.append({'size': [width, height], 'bounds': bounds})
        print(f'{width}x{height}: proportion-preserving HUD translation passed', flush=True)
    for skin in [int(x) for x in args.skins.split(',')]:
        if skin:
            capture(1280, 720, 'modded', skin)
    capture(1280, 720, 'modded', 0, 120)
    (output / 'results.json').write_text(json.dumps(reports, indent=2) + '\n')
    print(f'HUD captures: {output}')


if __name__ == '__main__':
    main()
