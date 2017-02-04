
set prg {
  struct Unit();

  proc main( Unit ~> out ; ; Unit) {
    ~out(Unit());
  };
}

fblc-test $prg main {} {
  get out Unit()
  return Unit()
}

