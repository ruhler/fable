set prg {
  # A union must not be declared multiple times.
  struct Unit();
  union A(Unit x, Unit y);
  union A(Unit a, Unit b, Unit c);

  proc main( ; ; Unit) {
    $(Unit());
  };
}
fblc-check-error $prg 5:9
