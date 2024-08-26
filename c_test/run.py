#!/usr/bin/env python3

import os
from pathlib import Path, PurePath
import subprocess
import shlex
import argparse
import sys

parse = argparse.ArgumentParser()
parse.add_argument('test', default=None, nargs='?')
parse.add_argument('-d', action='append')
parse.add_argument('-b', action='store', choices=[16, 32], default=32, type=int)

args = parse.parse_args()

c_test = Path(__file__).parent
os.chdir(c_test)

qnx = Path(os.environ['QNX_ROOT'])
slib_spec = shlex.split(os.environ['QNX_SLIB'])
qine = (c_test / '../build/qine').absolute()
build = (c_test / 'build').absolute()
cc =  '/bin/cc'
qine_cmd = [qine] + slib_spec + [
    '-m', f'/,{qnx}', 
    '-m', f'/t,{Path.cwd()},exec=qnx',
    '--']

build.mkdir(exist_ok=True)

class TestResult:
    def __init__(self, name: str, failures) -> None:
        self.name = name
        self.failures = failures

def print_args(args):
    print('+ ' + ' '.join([shlex.quote(str(v)) for v in args]))

def exec(args):
    print_args(args)
    subprocess.check_call(args)

test_results = []

def run_test(test, single=False):
    os.chdir(c_test)
    test = PurePath(test).stem
    test_dir = build / test
    test_dir.mkdir(exist_ok=True)
    os.chdir(test_dir)
    print(f'----- TEST {test} ------')

    if args.b == 32:
        switch = '-3'
    else:
        switch = '-2'

    exec(qine_cmd + [cc , switch, '-c', f'../../{test}.c'])
    exec(qine_cmd + [cc, switch, '-c', f'../../common.c'])
    exec(qine_cmd + [cc, switch,
        '-o', f'{test}', 'common.o', f'{test}.o', '-lunix'
    ])

    extra_args = []
    for a in args.d or []:
        extra_args.extend(['-d', a])

    failures = []
    run_args = [qine] + slib_spec + extra_args + ['--', f'./{test}']
    print_args(run_args)
    proc = subprocess.Popen(run_args, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, encoding='ascii')
    with open('run.log', 'wt') as f:
        for l in proc.stdout:
            f.write(l)
            sys.stdout.write(l)
            if l.startswith('no!'):
                failures.append(l.strip())
    r = proc.wait()
    if r != 0:
        failures.append('no! retcode\n')
        if single:
            print(f'Test failed with {r}')
            sys.exit(r)

    test_results.append(TestResult(test, failures))

if args.test is None:
    for t in Path('.').glob('*.c'):
        stem = t.stem
        if stem == 'common':
            continue
        run_test(stem)

    failing_results = [t for t in test_results if t.failures]
    print("----------")
    if len(failing_results) == 0:
        print("All tests passed")
    else:
        for t in failing_results:
            print(t.name)
            for r in t.failures:
                print(f'  {r}')
else:
    run_test(args.test, single=True)
