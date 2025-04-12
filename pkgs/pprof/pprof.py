
import sys

class Node:
    def __init__(self, name):
        self.total = 0
        self.self = 0
        self.name = name
        self.children = {}
                     

def Traverse(node, entry):
    node.total += 1
    if entry in node.children:
        child = node.children[entry]
        child.total += 1
        return child
    child = Node(entry)
    child.total += 1
    node.children[entry] = child
    return child

def Dump(node, indent):
    print("%s%s %d %d" % (indent, node.name, node.total, node.self))
    for entry in node.children:
        Dump(node.children[entry], indent + "  ")

root = Node("<root>")

node = root
for line in sys.stdin:
    if line.startswith('\t'):
        [addr, name, so] = line.split()
        entry = name.split('+')[0]
        node = Traverse(node, entry)
    elif len(line.strip()) == 0:
        node.self += 1
        node = root

Dump(root, "")
