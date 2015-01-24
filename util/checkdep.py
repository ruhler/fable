# Check the dependencies for a given .h or .cc file.
#
# Inputs: DEP ...
# Verify there is a one-to-one corresondence between #include "..." statements
# in the given file and the listed DEPs.
# Returns 0 if the dependencies are correct, and error otherwise.
#
# For example, given arguments: src/foo.h bar.h sludge.h, this will verify
# that src/foo.h contains #include "bar.h" and #include "sludge.h", and
# that it does not have any other local(between double quotes) includes.

import sys

file = sys.argv[1]

# Gather the dependencies listed in a set.
deps = {}
for d in sys.argv[2:]:
  deps[d] = 1

# Gather the includes listed in a set.
includes = {}
for line in open(file, 'r'):
  # We look for lines of the form: #include "foo.h"
  if line[0:10] == '#include "' and line[-2:] == '"\n':
    include = line[10:-2]
    includes[include] = 1

# Verify all dependencies are included in the file.
for d in deps:
  if not d in includes:
    print "error: %s does not include %s" % (file, d)
    sys.exit(1)

# Verify all included files are listed as dependencies
for i in includes:
  if not i in deps:
    print "error: %s includes %s" % (file, i)
    sys.exit(1)

sys.exit(0)

