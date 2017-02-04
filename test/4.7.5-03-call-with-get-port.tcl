set prg {
  # Test calling a process with a get port.
  struct Unit();
  union Bool(Unit true, Unit false);

  proc sub(Bool <~ get ; ; Bool) {
    ~get();
  };

  proc main( ; ; Bool) {
    Bool <~> myget, put;
    Bool putted = ~put(Bool:true(Unit()));
    sub(myget ; );
  };
}

expect_result Bool:true(Unit()) $prg main
