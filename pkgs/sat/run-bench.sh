# Usage: bash run-bench.sh fble-sat DIR
# Run fble-sat on all .cnf files in the given directory.
#
# For example, you could get benchmarks from:
#   https://www.cs.ubc.ca/~hoos/SATLIB/Benchmarks/SAT/New/Competition-02/sat-2002-beta.tgz
#
# An easy problem to start with:
#   sat-2002-beta/generated/gen-1/gen-1.1/unif-c1000-v500-s1356655268.cnf

for f in `find $2 -type f -name '*.cnf'`; do
  echo $f
  timeout 30s /usr/bin/time -f %E $1 < $f 
done
