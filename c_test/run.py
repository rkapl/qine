#!/usr/bin/env python3

import os
from pathlib import Path, PurePath
import subprocess
import shlex
import argparse

parse = argparse.ArgumentParser()
parse.add_argument('test')
parse.add_argument('-d', action='append')

args = parse.parse_args()

c_test = Path(__file__).parent
os.chdir(c_test)

qnx = Path(os.environ['QNX_ROOT'])
watcom = qnx / 'usr/watcom/10.6'
qine = c_test / '../build/qine'
build = c_test / 'build'

build.mkdir(exist_ok=True)

def exec(args):
    print('+ ' + ' '.join([shlex.quote(str(v)) for v in args]))
    subprocess.check_call(args)

def run_test(test):
    os.chdir(c_test)
    test = PurePath(test).stem
    test_dir = build / test
    test_dir.mkdir(exist_ok=True)
    os.chdir(test_dir)
    qine_cmd = [qine , '--']

    includes = [
        '-I' + str(watcom / 'include'),
        '-I' + str(qnx / 'usr/include'),
    ]

    exec(qine_cmd + [watcom / 'bin/wcc386', f'../../{test}.c'] + includes)
    exec(qine_cmd + [watcom / 'bin/wlink', 
        'FORM', 'qnx', 'flat', 'NAME', test, 'OP', 'q', 'FILE', f'{test}.o', 
        'LIBP', ':'.join([
            str(watcom / 'include'),
            str(qnx / 'usr/lib')
        ])
    ])

    extra_args = []
    for a in args.d or []:
        extra_args.extend(['-d', a])

    exec([qine] + extra_args + ['--', f'./{test}'])

run_test(args.test)