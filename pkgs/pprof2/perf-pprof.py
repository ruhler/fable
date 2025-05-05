
# Converts output of 'perf script' into fble text pprof based format.
# Usage: perf script | python3 perf-pprof.py > foo.pprof.txt

import sys

# Removes cycles of length n from the given trace
def RemoveCycles(trace, n):
    i = 0
    while i < len(trace):
        if trace[i:i+n] == trace[i+n:i+2*n]:
            for k in range(0, n):
                trace.pop(i+n)
        else:
            i += 1

def Canonicalize(trace):
    # To canonocalize a stack trace, any time we see a repeated subsequence
    # abc,abc in the trace, drop the second occurence.
    for i in range(1, len(trace)):
        RemoveCycles(trace, i)

total = 0       # Total number of samples.
seqs = {}       # Full sequence to count of occurences.

sample = []
for line in sys.stdin:
    if line.startswith('\t'):
        [addr, name, so] = line.split()
        entry = name.split('+')[0]
        sample.append(entry)
    elif len(line.strip()) == 0:
        sample.reverse()
        Canonicalize(sample)
        seq = ';'.join(sample)
        seqs[seq] = 1 + seqs.get(seq, 0)
        sample = []
        total += 1
        print("\rsamples: %d" % total, end='', flush=True, file=sys.stderr)

print("", file=sys.stderr)

for seq in sorted(seqs.keys(), key=lambda x: -seqs[x]):
    print("%d %s" % (seqs[seq], seq))
