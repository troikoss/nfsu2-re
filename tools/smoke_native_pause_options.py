#!/usr/bin/env python3
"""Exercise the native settings integration with the player's retail FNG assets.

This checks native routing and session state, not original/rebuilt equivalence.
Run the PC verifiers separately for recovered-code evidence. Requires a working
SDL/OpenGL display; screenshots and logs stay in ignored build/analysis.
"""
import argparse
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
CASES = {
    'audio': ('RAALDL', ('name=Master Volume data= slider=0.95', 'name=Menu Music Volume data= slider=0.95')),
    'display': ('RARARDR', ('name=Resolution data=1024x768', 'name=Level Of Detail data= slider=4.00')),
    'gameplay': ('RARRADDR', ('name=Speedometer Units data=KM/H', 'units=1')),
    'player': ('RARRRADDRDR', ('name=Camera data=Bumper', 'camera=0 gauges=0')),
    'save_prompt': ('RARRADDDDDRB', ('name=Save data=Off', 'pause: dialog prompt 22bc94af')),
    'save_accept': ('RARRADDDDDR BLA'.replace(' ', ''), ('pause: dialog prompt 22bc94af', '<- 11111111')),
    'save_cancel': ('RARRADDDDDR BB'.replace(' ', ''), ('pause: dialog prompt 22bc94af', '<- 22222222')),
    'display_dialog': ('RARADDDAB', ('pause: dialog prompt 2bbcca6b', '<- 34dc1bcf')),
    'return_and_resume': ('RAABLBS', ('pause: showing UI_Pause.fng', 'pause: game flow 0x5f4240(1)')),
    'reopen_category': ('RAALBAL', ('name=Master Volume data= slider=0.95', 'name=Master Volume data= slider=0.90')),
    'player_scroll': ('RARRRA' + 'D' * 10 + 'RUR', ('edit row=10 name=Score data=Off', 'edit row=9 name=Leaderboard data=Off')),
    'slider_full': ('RAAR', ('name=Master Volume data= slider=1.00',)),
    'slider_half': ('RAA' + 'L' * 10, ('name=Master Volume data= slider=0.50',)),
    'slider_empty': ('RAA' + 'L' * 20, ('name=Master Volume data= slider=0.00',)),
    'slider_clamp': ('RAA' + 'L' * 22 + 'R', ('name=Master Volume data= slider=0.00', 'name=Master Volume data= slider=0.05')),
    'display_reset': ('RARADDDALA', ('pause: dialog prompt 2bbcca6b', 'retail display reset is not recovered', '<- af0bbd92')),
    'player_without_hud': ('RARRRADDR', ('name=Camera data=Bumper', 'camera=0')),
}


def check_slider_pixels(out):
    """Check rendered fill motion, including the vertical offset regression."""
    widths, knobs = [], []
    for name in ('slider_empty', 'slider_half', 'slider_full'):
        magic, dimensions, maximum, pixels = (out / (name + '.ppm')).read_bytes().split(b'\n', 3)
        width, height = map(int, dimensions.split())
        assert magic == b'P6' and maximum == b'255' and len(pixels) == width * height * 3
        fill, bright = [], []
        for y in range(int(height * .21), int(height * .28)):
            for x in range(width // 2, int(width * .79)):
                r, g, b = pixels[(y * width + x) * 3:(y * width + x) * 3 + 3]
                if abs(r - 99) <= 2 and abs(g - 178) <= 2 and abs(b - 34) <= 2:
                    fill.append((x, y))
                if width * .54 < x < width * .77 and height * .22 < y < height * .245 and min(r, g, b) > 210:
                    bright.append(x)
        assert bright, f'{name}: slider knob missing'
        knobs.append((min(bright) + max(bright)) / 2)
        widths.append(max(x for x, y in fill) - min(x for x, y in fill) + 1 if fill else 0)
        if fill:
            # The detached fill used to lie below the first row's track, around y=.25h.
            assert max(y for x, y in fill) < height * .245, f'{name}: fill below track'
            assert abs(max(x for x, y in fill) - knobs[-1]) < height * .016, f'{name}: knob detached from fill'
    assert widths[0] == 0, f'empty slider still filled: {widths}'
    assert widths[2] > 0 and abs(widths[1] / widths[2] - .5) < .03, f'fill movement mismatch: {widths}'
    assert knobs[0] < knobs[1] < knobs[2], f'knob did not move: {knobs}'
    assert abs((knobs[1] - knobs[0]) / (knobs[2] - knobs[0]) - .5) < .03, f'knob movement mismatch: {knobs}'
    print(f'PASS slider rendered fill widths: {widths}; knob centres: {knobs}')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--demo', type=Path, default=ROOT / 'build/native/ug2demo')
    parser.add_argument('--game', type=Path, default=ROOT / 'Need for Speed Underground 2')
    args = parser.parse_args()
    out = ROOT / 'build/analysis/native-options'
    out.mkdir(parents=True, exist_ok=True)
    for name, (keys, expected) in CASES.items():
        image = out / (name + '.ppm')
        image.unlink(missing_ok=True)
        cmd = [str(args.demo), '--game', str(args.game), '--car', 'SKYLINE', '--frames', str(80 + 30 * len(keys)),
               '--pause-at', '20', '--pause-keys', keys, '--screenshot', str(image)]
        if name == 'player_without_hud':
            cmd.append('--no-hud')
        result = subprocess.run(cmd, env=dict(os.environ, UG2_DEBUG_PAUSE='1'), capture_output=True, text=True, timeout=90)
        log = result.stdout + result.stderr
        (out / (name + '.log')).write_text(log)
        assert result.returncode == 0, f'{name}: demo exit {result.returncode}, see {out / (name + ".log")}'
        assert image.exists() and image.read_bytes().startswith(b'P6\n'), f'{name}: screenshot missing'
        for text in expected:
            assert text in log, f'{name}: missing {text!r}, see {out / (name + ".log")}'
        assert 'unrecovered original code' not in log, f'{name}: reached trap'
        print(f'PASS {name}: {keys}')
    check_slider_pixels(out)
    print(f'PASS {len(CASES)} native settings scenarios (routing/session state only)')


if __name__ == '__main__':
    main()
