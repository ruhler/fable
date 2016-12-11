
set prg {
  struct Unit();
  union Bool(Unit true, Unit false);

  func notproc( ; Bool) {
    Bool:true(Unit());
  };

  proc main( ; ; Bool) {
    // notproc is not a proc.
    notproc( ; );
  };
}

expect_malformed $prg main
expect_malformed_b $prg 3
