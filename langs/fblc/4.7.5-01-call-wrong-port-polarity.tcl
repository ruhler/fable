set prg {
  # Ports passed to a call must have the right polarity.
  struct Unit();

  proc put(Unit+ out; ; Unit) {
    +out(Unit());
  };

  proc main( ; ; Unit) {
    Unit +- p_put,  p_get;
    put(p_get; );
  };
}
fblc-check-error $prg 11:9
