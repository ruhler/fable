set prg {
  struct Unit();
  union Bool(Unit true, Unit false);

  proc sub(Bool <~ get ; ; Bool) {
    ~get();
  };

  proc main( ; ; Bool) {
    # The first port argument is not in scope.
    sub(foo ; );
  };
}
fblc-check-error $prg 11:9
