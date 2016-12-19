
set prg {
  struct Unit();

  proc main( Unit <~ in ; ; Unit) {
    in~();
  };
}

expect_proc_result "" $prg 1 {{i in}} {} { put in 0 }

