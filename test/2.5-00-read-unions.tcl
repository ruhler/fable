
set prg {
  struct Unit();
  union Bool(Unit True, Unit False);

  proc main( Bool <~ in ; ; Bool) {
    Bool x = in~();
    in~();
  };
}

# Verify we can read two union values in a row.
expect_proc_result 0 $prg 2 {{i in}} {} {
  put in 1
  put in 0
}

