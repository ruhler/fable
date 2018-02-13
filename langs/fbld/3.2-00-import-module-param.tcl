set prg {
  module PairM<type T> {
    # The type T is not in scope. It must be explicitly imported using an
    # import @ statement.
    struct Pair(T first, T second);
  };
}

fbld-check-error $prg 5:17
