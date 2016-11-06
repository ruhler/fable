
set prg {
  struct Unit();

  proc main( Unit <~ in ; ; Unit) {
    in~();
  };
}

expect_proc_result Unit() $prg main {{i in}} {} { put in Unit() }

