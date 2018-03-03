set prg {
  module StdLib {
    struct Unit();
    abst struct Pair(Unit first, Unit second);
    func MkPair( ; Pair) {
      Pair(Unit(), Unit());
    };
  };

  import StdLib { Unit; Pair; MkPair; };

  func main( ; Unit) {
    # The 'first' field of Pair cannot be accessed because it is an abstract type.
    MkPair().first;
  };
}

fbld-check-error $prg 14:14
