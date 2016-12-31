set prg {
  // A struct must not be declared multiple times.
  struct Unit();
  struct A(Unit x, Unit y);
  struct A(Unit a, Unit b, Unit c);

  proc main( ; ; Unit) {
    $(Unit());
  };
}
fblc-check-error $prg 5:10
