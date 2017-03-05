set prg {
  struct Unit();

  proc main( Unit <~ in ; ; Unit) {
    ~in();
  };
}
fblc-test $prg main {} {
  put in Unit()
  return Unit()
}
