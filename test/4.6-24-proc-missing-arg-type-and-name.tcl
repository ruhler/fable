set prg {
  struct Unit();

  # An entire argument is missing in the list.
  proc p(Unit <~ px, Unit ~> py ; Unit x, , Unit z; Unit) {
    $(Unit());
  };
}
fblc-check-error $prg 5:43
