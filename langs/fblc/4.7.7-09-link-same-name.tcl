set prg {
  struct Unit();
  union Bool(Unit true, Unit false);

  proc main( ; ; Bool) {
    # You can't use the same name for get and put ports in a link action.
    Bool <~> p, p;
    ~p(Bool:true(Unit()));
  };
}
fblc-check-error $prg 7:17
