set prg {
  struct Unit();

  # An entire argument is missing from the argument list.
  func f(Unit x, , Unit z; Unit) {
    x;
  };
}
fblc-check-error $prg 5:18
