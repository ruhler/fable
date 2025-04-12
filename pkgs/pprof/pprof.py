
import sys

samples = []
sample = []

for line in sys.stdin:
    if line.startswith('\t'):
        [addr, name, so] = line.split()
        sample.append(name.split('+')[0])
    else:
        if len(sample) > 0:
            samples.append(sample)
            sample = []

print(samples)

