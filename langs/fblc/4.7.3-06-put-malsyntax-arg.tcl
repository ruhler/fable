set prg {
  struct Unit();

  proc f(Unit+ myput ; ; Unit) {
    # The argument to the port has invalid syntax.
    +myput(???);
  };
}
fblc-check-error $prg 6:13
