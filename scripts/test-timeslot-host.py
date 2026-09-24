"""Execute dependency C, with boundary mocks; this is not a hardware simulation.

Requires a pinned checkout with the RADIO patch already applied. Builds an
unmodified control from git and the current source. Negative controls must fail
specific assertions rather than crashing for an unrelated reason.
"""
import argparse
from pathlib import Path
import re
import subprocess
import tempfile

p = argparse.ArgumentParser()
p.add_argument('esb', type=Path)
args = p.parse_args()
root = Path(__file__).resolve().parent.parent
rel = 'src/split/esb/timeslot.c'
revision = subprocess.check_output(['git', '-C', str(args.esb), 'rev-parse', 'HEAD'], text=True).strip()
assert revision == '314c7cbaf4a74e1add1d6ffc8249de3e29965b8c', revision
original = subprocess.check_output(['git', '-C', str(args.esb), 'show', f'HEAD:{rel}'], text=True)
current = (args.esb / rel).read_text()
stubs = (root / 'tests/timeslot_host_stubs.h').read_text()
cases = (root / 'tests/timeslot_host_cases.c').read_text()
fixes = ['late_start', 'close_timer', 'close_extend', 'no_event', 'open_failure', 'close_idle']
regressions = ['extend_success', 'extend_failure']
session_cases = ['reopen_waits', 'busy_request', 'missing_session', 'close_retry',
                 'close_already_closed', 'coalesced_requests']
with tempfile.TemporaryDirectory() as temp:
    for label, source in [('upstream', original), ('patched', current)]:
        # Only replace includes with mocks; execute the dependency implementation.
        source = re.sub(r'^#include[^\n]*', '', source, flags=re.M)
        file = Path(temp) / f'{label}.c'
        exe = Path(temp) / label
        define = '#define SESSION_LIFECYCLE_PROBE 1\n' if label == 'patched' else ''
        file.write_text(define + stubs + '\n' + source + '\n' + cases)
        command = ['gcc', '-std=c11', '-O2', '-Wall', '-Werror=implicit-function-declaration',
                   '-Werror=int-conversion', str(file), '-o', str(exe)]
        subprocess.run(command, check=True, capture_output=True, text=True)
        for case in fixes + regressions + (session_cases if label == 'patched' else []):
            result = subprocess.run([str(exe), case], capture_output=True, text=True)
            if label == 'upstream' and case in fixes:
                expected = 'requested_id' if case in ['open_failure', 'close_idle'] else 'signal_action'
                assert result.returncode != 0 and 'Assertion' in result.stderr and expected in result.stderr, (case, result)
                print(f'CONFIRMED upstream failure: {case}', flush=True)
            else:
                assert result.returncode == 0, (label, case, result.stdout, result.stderr)
                print(label + ': ' + result.stdout.strip(), flush=True)
        invalid = subprocess.run(command + ['-DCONFIG_ZMK_SPLIT_ESB_TIMESLOT_LENGTH_US=1000'],
                                 capture_output=True, text=True)
        if label == 'patched':
            assert invalid.returncode != 0 and 'timeslot length must exceed' in invalid.stderr, invalid.stderr
            print('PASS invalid 1 ms length rejected at compile time', flush=True)
        else:
            assert invalid.returncode == 0, invalid.stderr
            print('CONFIRMED upstream accepts timer-margin underflow configuration', flush=True)
print('Host control-flow checks complete. Radio/IRQ concurrency and hardware remain unverified.')
