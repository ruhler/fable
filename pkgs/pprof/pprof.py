
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

def Dump(node, indent):
    print("%s%s %d %d" % (indent, node.name, node.total, node.self))
    children = sorted(node.children.values(), key=lambda x: -x.total)
    for child in children:
        Dump(child, indent + "  ")

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


root = Node("<root>")
overalls = {}    # Count 'overall' sample appearence of each entry.
selfs = {}      # Count 'self' sample apparence of each entry.
total = 0       # Total number of samples.

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

#Total(root)
#Dump(root, "")
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
