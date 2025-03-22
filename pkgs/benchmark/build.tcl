namespace eval "pkgs/benchmark" {
  pkg benchmark [list core md5] "" ""

  # fble-benchmark
  stdio $::b/pkgs/benchmark/fble-benchmark "/Benchmark/Main%" "md5 benchmark" ""
  install $::b/pkgs/benchmark/fble-benchmark $::config::bindir/fble-benchmark
}
