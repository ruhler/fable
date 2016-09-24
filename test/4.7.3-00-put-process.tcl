
set prg {
  struct Unit();

  proc main( Unit ~> out ; ; Unit) {
    out~(Unit());
  };
}

expect_proc_result Unit() $prg main {{o out}} {} { get out Unit() }

