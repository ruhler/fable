set prg {
  struct Unit();
  union Bool(Unit true, Unit false);

  proc main( ; ; Bool) {
    # The process is type Unit, but type Bool is expected.
    Bool x = $(Unit());
    $(x);
  };
}
# TODO: Where exactly should the error be?
fblc-check-error $prg 7:5
