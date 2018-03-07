set prg {
  # Can't have two static parameters with the same name.
  struct Pair<type A, type A>(A first, A second);
}

fbld-check-error $prg 3:27
