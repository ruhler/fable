set prg {
  struct Unit();
  union Bool(Unit true, Unit false);

  proc sub(Bool <~ get ; ; Bool) {
    get~();
  };

  proc main( ; ; Bool) {
    Bool <~> myget, put;
    Bool putted = put~(Bool:true(Unit()));
    // The second port argument has the wrong syntax. 
    sub(myget, ??? ; );
  };
}

expect_malformed $prg main
expect_malformed_b $prg 3

