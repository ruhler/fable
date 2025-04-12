
import sys

class Node:
    def __init__(self, name):
        self.self = 0
        self.name = name
        self.children = {}
                     

def Traverse(node, entry):
    if entry in node.children:
        child = node.children[entry]
        return child
    child = Node(entry)
    node.children[entry] = child
    return child

def Dump(node, indent):
    print("%s%s %d" % (indent, node.name, node.self))
    for entry in node.children:
        Dump(node.children[entry], indent + "  ")

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
            node = Traverse(node, entry)
        node.self += 1
        samples = []

Dump(root, "")
