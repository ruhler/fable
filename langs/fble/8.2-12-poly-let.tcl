fble-test {
  @ Unit@ = *();

  # Regression test for when we had a memory leak of a ref type when doing
  # poly-substition of ref types.
  <@>@ Foo@ = <@ T@> {
    @ Bar@ = *(T@ x);
    *(@<Bar@> Bar@, Bar@ bar);
  };

  @ UnitBar@ = *(Unit@ x);
  Foo@<Unit@>(UnitBar@, UnitBar@(Unit@()));
}
