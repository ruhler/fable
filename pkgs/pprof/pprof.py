
import sys

class Node:
    def __init__(self, name):
        self.total = 0
        self.self = 0
        self.name = name
        self.children = {}
                     
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

root = Node("<root>")

samples = []
for line in sys.stdin:
    if line.startswith('\t'):
        [addr, name, so] = line.split()
        entry = name.split('+')[0]
        samples.append(entry)
    elif len(line.strip()) == 0:
        samples.reverse()
        node = root
        for entry in samples:
            node = InsertInto(node, entry)
        node.self += 1
        samples = []

Total(root)
Dump(root, "")
