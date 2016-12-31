set prg {
  struct Unit();

  proc main( ; ; Unit) {
    // myget port is not in scope.
    myget~();
  };
}
fblc-check-error $prg 6:5
