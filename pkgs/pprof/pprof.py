
import sys

class Node:
    def __init__(self, name):
        self.total = 0
        self.self = 0
        self.name = name
        self.children = {}

class Entry:
    def __init__(self, name):
        self.name = name
        self.count = 0
                     
def InsertInto(node, entry):
    if entry in node.children:
        child = node.children[entry]
        return child
    child = Node(entry)
    node.children[entry] = child
    return child

def Total(node):
    total = node.self
    for entry in node.children:
        total += Total(node.children[entry])
    node.total = total
    return total

total = 0       # Total number of samples.

def Dump(node, indent):
    print("%s%8.2f%% % 8d %8.2f%%  % 8d %s" % (
        indent,
        100.0 * node.total / total, node.total,
        100.0 * node.self / total, node.self,
        node.name))
    children = sorted(node.children.values(), key=lambda x: -x.total)
    for child in children:
        Dump(child, indent + "  ")

canon_stats = {}

# Removes cycles of length n from the given trace
def RemoveCycles(trace, n):
    i = 0
    while i < len(trace):
        if trace[i:i+n] == trace[i+n:i+2*n]:
            if n in canon_stats:
                canon_stats[n].count += 1
            else:
                canon_stats[n] = Entry("% 8d" % n)
                canon_stats[n].count = 1
            for k in range(0, n):
                trace.pop(i+n)
        else:
            i += 1

def Canonicalize(trace):
    # To canonocalize a stack trace, any time we see a repeated subsequence
    # abc,abc in the trace, drop the second occurence.
    for i in range(1, len(trace)):
        RemoveCycles(trace, i)


root = Node("<root>")
overalls = {}    # Count 'overall' sample appearence of each entry.
selfs = {}      # Count 'self' sample apparence of each entry.

canon = {}
focus = None
samples = []
appends = {}
for line in sys.stdin:
    if line.startswith('\t'):
        [addr, name, so] = line.split()
        entry = name.split('+')[0]
        samples.append(entry)
    elif len(line.strip()) == 0:
        samples.reverse()

        if focus:
            try:
                i = samples.index(focus)
                samples = samples[i:]
            except ValueError:
                continue

        Canonicalize(samples)
        canonized = ';'.join(samples)
        if canonized in canon:
            canon[canonized].count += 1
        else:
            canon[canonized] = Entry(canonized)
            canon[canonized].count += 1

        total += 1
        node = root
        entries = {}
        for entry in samples:
            node = InsertInto(node, entry)
            entries[entry] = True
        for entry in entries:
            if entry in overalls:
                overalls[entry].count += 1
            else:
                overalls[entry] = Entry(entry)
                overalls[entry].count += 1
        node.self += 1
        if node.name in selfs:
            selfs[node.name].count += 1
        else:
            selfs[node.name] = Entry(node.name)
            selfs[node.name].count += 1
        samples = []

print("By Overall")
print("==========")
for entry in sorted(overalls.values(), key=lambda x: -x.count):
    print("%8.2f%% % 8d %s" % (100.0 * entry.count / float(total),
        entry.count, entry.name))

print("")
print("By Self")
print("=======")
for entry in sorted(selfs.values(), key=lambda x: -x.count):
    print("%8.2f%% % 8d %s" % (100.0 * entry.count / float(total),
        entry.count, entry.name))

print("")
print("By Canonical")
print("============")
for entry in sorted(canon.values(), key=lambda x: -x.count):
    print("%8.2f%% % 8d %s" % (100.0 * entry.count / float(total),
        entry.count, entry.name))

print("")
print("Canon Tree")
print("==========")
Total(root)
Dump(root, "")

print("")
print("Canonicalization Stats")
print("======================")
for entry in sorted(canon_stats.values(), key=lambda x: -x.count):
    print("%s: % 8d" % (entry.name, entry.count))

