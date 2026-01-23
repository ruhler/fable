namespace eval "pkgs/benchmark" {
  pkg benchmark [list std core app fbld games graphics invaders md5 pinball sat] "" ""

  # fble-benchmark
  cli $::b/pkgs/benchmark/fble-benchmark "/Benchmark/Main%" "app fbld games graphics invaders md5 pinball sat benchmark" ""
  install $::b/pkgs/benchmark/fble-benchmark $::config::bindir/fble-benchmark
}
