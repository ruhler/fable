
set prg {
  struct Unit();

  proc main( Unit ~> out ; ; Unit) {
    out~(Unit());
  };
}

expect_proc_result "" $prg 1 {{o out}} {} { get out 0 }

