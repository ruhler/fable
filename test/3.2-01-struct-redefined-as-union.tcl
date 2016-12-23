

# A union and a struct can't have the same name.
set prg {
  struct Unit();
  struct A(Unit x, Unit y);
  union A(Unit a, Unit b, Unit c);

  proc main( ; ; Unit) {
    $(Unit());
  };
}
fblc-check-error $prg

