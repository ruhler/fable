set prg {
  struct Unit();

  proc f(Unit ~> myput ; ; Unit) {
    # The argument to the port has invalid syntax.
    ~myput(???);
  };

  proc main( ; ; Unit) {
    $(Unit());
  };
}
fblc-check-error $prg 6:13
