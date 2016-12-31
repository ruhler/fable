set prg {
  // Process port arguments must have a name.
  struct Unit();

  proc p(Unit <~ , Unit ~> py ; Unit x, Unit y; Unit) {
    $(Unit());
  };

  proc main( ; ; Unit) {
    $(Unit());
  };
}
fblc-check-error $prg 5:18
