#!/usr/bin/env python

import os, sys, subprocess, difflib

from scripts.test.support import run_command, split_wast

print '[ processing and updating testcases... ]\n'

print '\n[ checking wasm-ctor-eval... ]\n'

for t in os.listdir(os.path.join('test', 'ctor-eval')):
  if t.endswith(('.wast', '.wasm')):
    print '..', t
    t = os.path.join('test', 'ctor-eval', t)
    ctors = open(t + '.ctors').read().strip()
    for opt in [0]:#, 1]:
      cmd = [os.path.join('bin', 'wasm-ctor-eval'), t, '-o', 'a.wast', '-S', '--ctors', ctors]
      if opt: cmd += ['-O']
      stdout = run_command(cmd)
      actual = open('a.wast').read()
      out = t + '.out'
      if opt: out += '.opt'
      with open(out, 'w') as o: o.write(actual)

print '\n[ success! ]'
