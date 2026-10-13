#!/usr/bin/env python3
"""Collect the viewer's fixed 5s warmup/60s capture and private device snapshots."""
import argparse
import csv
import hashlib
import io
import json
from pathlib import Path
import re
import subprocess
import time

PACKAGE = 'com.proevolutica.viewer'
ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--serial', required=True, help='Authorized ADB device serial')
    parser.add_argument('--output', type=Path, required=True, help='New private output directory')
    parser.add_argument('--runs', type=int, default=3, choices=range(1, 6))
    parser.add_argument('--apk', type=Path, default=ROOT/'android/app/build/outputs/apk/debug/app-debug.apk')
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)

    def adb(*arguments):
        return subprocess.run(['adb', '-s', args.serial, *arguments], check=True,
                              stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=30).stdout

    def snapshot(folder, phase):
        for name, command in [('memory', ['meminfo', PACKAGE]), ('thermal', ['thermalservice']),
                              ('battery', ['battery'])]:
            (folder/f'{phase}-{name}.txt').write_bytes(adb('shell', 'dumpsys', *command))

    apk_hash = hashlib.sha256(args.apk.read_bytes()).hexdigest()
    package_path = adb('shell', 'pm', 'path', PACKAGE).decode().strip()
    if not re.fullmatch(r'package:/data/app/[A-Za-z0-9_./=~+\-]+/base\.apk', package_path):
        raise RuntimeError('Expected one installed APK path')
    installed_hash = adb('shell', 'sha256sum', package_path.removeprefix('package:')).decode().split()[0]
    if installed_hash != apk_hash:
        raise RuntimeError('Installed APK does not match --apk; install the intended build first')
    manifest = {'apk_sha256': apk_hash, 'runs': args.runs, 'warmup_seconds': 5,
                'requested_seconds': 60, 'memory_sampling': 'before warmup and after report; outside capture'}
    (args.output/'manifest.json').write_text(json.dumps(manifest, indent=2)+'\n')
    (args.output/'asset-hashes.txt').write_bytes(adb('shell', 'run-as', PACKAGE, 'sha256sum',
                                                   'files/model.obj', 'files/model.png'))
    (args.output/'display.txt').write_bytes(adb('shell', 'dumpsys', 'display'))
    for index in range(1, args.runs+1):
        folder = args.output/f'run-{index}'
        folder.mkdir()
        adb('shell', 'am', 'force-stop', PACKAGE)
        (folder/'launch.txt').write_bytes(adb('shell', 'am', 'start', '-W', '-n', PACKAGE+'/.ViewerActivity'))
        time.sleep(2)  # Let initial resize/input events settle before starting capture.
        pid = adb('shell', 'pidof', PACKAGE).decode().strip()
        if not pid.isdigit():
            raise RuntimeError('Expected one viewer process')
        snapshot(folder, 'before')
        adb('shell', 'input', 'keyevent', '30')  # Android B -> SDL SDLK_B.
        print(f'Run {index}: 5s warmup + 60s capture (PID {pid})', flush=True)
        time.sleep(67)
        log = adb('logcat', '-d', '--pid='+pid, '-v', 'threadtime',
                  '-s', 'SDL', 'SDL/APP', 'AndroidRuntime')
        (folder/'logcat.txt').write_bytes(log)
        if (adb('shell', 'pidof', PACKAGE).decode().strip() != pid
                or b'Benchmark started:' not in log or b'Benchmark complete:' not in log
                or b'Benchmark aborted:' in log):
            raise RuntimeError('Capture interrupted, restarted or not completed')
        report_bytes = adb('exec-out', 'run-as', PACKAGE, 'cat', 'files/benchmark.json')
        frames_bytes = adb('exec-out', 'run-as', PACKAGE, 'cat', 'files/benchmark-frames.csv')
        report = json.loads(report_bytes)
        rows = list(csv.DictReader(io.StringIO(frames_bytes.decode())))
        if (report.get('schema') != 1 or report.get('status') != 'complete' or report.get('scene') != 'mesh'
                or report.get('requested_seconds') != 60 or report.get('seconds', 0) < 60
                or report.get('frames') != len(rows)):
            raise RuntimeError('Missing, partial or unexpected mesh capture')
        (folder/'benchmark.json').write_bytes(report_bytes)
        (folder/'frames.csv').write_bytes(frames_bytes)
        snapshot(folder, 'after')
        print(f"Run {index}: {report['fps']:.3f} FPS; p99 {report['interval']['p99_ms']:.3f} ms; "
              f"max {report['interval']['max_ms']:.3f} ms", flush=True)


if __name__ == '__main__':
    try:
        main()
    except (OSError, ValueError, RuntimeError, subprocess.SubprocessError) as error:
        raise SystemExit(f'Benchmark collection failed: {error}') from error
