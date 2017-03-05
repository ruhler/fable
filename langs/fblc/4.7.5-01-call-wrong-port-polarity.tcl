set prg {
  # Ports passed to a call must have the right polarity.
  struct Unit();

  proc put(Unit ~> out; ; Unit) {
    ~out(Unit());
  };

  proc main( ; ; Unit) {
    Unit <~> p_get, p_put;
    put(p_get; );
  };
}
# TODO: Fix the error location
fblc-check-error $prg 11:9
