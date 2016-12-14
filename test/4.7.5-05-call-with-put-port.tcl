
# Test process call that takes a put port.
set prg {
  struct Unit();
  union Bool(Unit true, Unit false);

  proc sub(Bool ~> put ; ; Unit) {
    Bool putted = put~(Bool:true(Unit()));
    $(Unit());
  };

  proc main( ; ; Bool) {
    Bool <~> myget, myput;
    Unit blah = sub(myput ; );
    myget~();
  };
}

expect_result Bool:true(Unit()) $prg main
# skip: requires support for procs
skip expect_result_b "0" $prg 3

