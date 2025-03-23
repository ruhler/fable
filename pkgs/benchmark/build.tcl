namespace eval "pkgs/benchmark" {
  pkg benchmark [list core app games graphics invaders md5 pinball pprof] "" ""

  # fble-benchmark
  stdio $::b/pkgs/benchmark/fble-benchmark "/Benchmark/Main%" "app games graphics invaders md5 pinball pprof benchmark" ""
  install $::b/pkgs/benchmark/fble-benchmark $::config::bindir/fble-benchmark
}
