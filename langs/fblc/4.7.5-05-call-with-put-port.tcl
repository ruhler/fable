set prg {
  # Test process call that takes a put port.
  struct Unit();
  union Bool(Unit true, Unit false);

  proc sub(Bool ~> put ; ; Unit) {
    Bool putted = ~put(Bool:true(Unit()));
    $(Unit());
  };

  proc main( ; ; Bool) {
    Bool <~> myget, myput;
    Unit blah = sub(myput ; );
    ~myget();
  };
}

fblc-test $prg main {} "return Bool:true(Unit())"
