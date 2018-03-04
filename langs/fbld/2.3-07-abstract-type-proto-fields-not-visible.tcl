set prg {
  struct Unit();

  func main<type Pair>(Pair x; Unit) {
    # The 'first' field of Pair cannot be accessed because it is an abstract type.
    x.first;
  };
}

fbld-check-error $prg 6:7
