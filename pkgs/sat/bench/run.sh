# Usage: bash run.sh fble-sat DIR
# Run fble-sat on all .cnf files in the given directory.

for f in `find $2 -type f -name '*.cnf'`; do
  echo $f
  timeout 5m /usr/bin/time -f %E $1 < $f 
done
