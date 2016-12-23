

# A struct and a process can't have the same name.
set prg {
  struct Unit();
  union A(Unit x, Unit y);

  proc A( ; Unit a, Unit b ; Unit) {
    $(Unit());
  };

  proc main( ; ; Unit) {
    $(Unit());
  };
}
fblc-check-error $prg

