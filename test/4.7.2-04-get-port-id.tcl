set prg {
  # Test that references to get ports are properly linked with their source.
  struct Unit();
  union Bool(Unit true, Unit false);

  proc sub(Bool ~> put, Bool <~ get; ; Bool) {
    ~get();
  };

  proc main( ; ; Bool) {
    Bool <~> g1, p1;
    Bool <~> g2, p2;
    Bool x1 = ~p1(Bool:true(Unit()));
    Bool x2 = ~p2(Bool:false(Unit()));
    sub(p1, g2 ; ); 
  };
}

expect_result Bool:false(Unit()) $prg main
