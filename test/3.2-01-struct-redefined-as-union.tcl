set prg {
  // A union and a struct can't have the same name.
  struct Unit();
  struct A(Unit x, Unit y);
  union A(Unit a, Unit b, Unit c);

  proc main( ; ; Unit) {
    $(Unit());
  };
}
fblc-check-error $prg 5:9
