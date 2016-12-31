set prg {
  struct Unit();

  proc f(Unit ~> myput ; ; Unit) {
    // The variable 'x' is not not declared.
    myput~(x);
  };

  proc main( ; ; Unit) {
    $(Unit());
  };
}
fblc-check-error $prg 6:12
