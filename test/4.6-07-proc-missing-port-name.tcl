
# Process port arguments must have a name.
set prg {
  struct Unit();

  proc p(Unit <~ , Unit ~> py ; Unit x, Unit y; Unit) {
    $(Unit());
  };

  proc main( ; ; Unit) {
    $(Unit());
  };
}
expect_malformed $prg main

