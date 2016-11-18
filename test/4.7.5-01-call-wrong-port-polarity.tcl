# Ports passed to a call must have the right polarity.
set prg {
  struct Unit();

  proc put(Unit ~> out; ; Unit) {
    out~(Unit());
  };

  proc main( ; ; Unit) {
    Unit <~> p_get, p_put;
    put(p_get; );
  };
}
expect_malformed $prg main

