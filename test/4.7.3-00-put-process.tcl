
set prg {
  struct Unit();

  proc main( Unit ~> out ; ; Unit) {
    ~out(Unit());
  };
}

fblc-test Unit() $prg main {} { get out Unit() }

