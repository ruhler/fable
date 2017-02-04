set prg {
  struct Unit();

  proc main( Unit <~ in ; ; Unit) {
    ~in();
  };
}
fblc-test Unit() $prg main {} { put in Unit() }
