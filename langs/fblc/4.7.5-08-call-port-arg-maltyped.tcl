set prg {
  struct Unit();
  union Bool(Unit true, Unit false);

  proc sub(Unit <~ get ; ; Unit) {
    ~get();
  };

  proc main( ; ; Unit) {
    Bool <~> myget, put;
    Bool putted = ~put(Bool:true(Unit()));

    # sub takes a port of type Unit, not Bool.
    sub(myget ; );
  };
}
fblc-check-error $prg 14:9
