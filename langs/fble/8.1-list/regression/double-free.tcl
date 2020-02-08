fble-test {
  # Regression test that previously triggered a double free in the compiler
  # due to mismanagement of the lifetime of a kind synthesized for the list
  # expression.
  @ Unit@ = *();
  Unit@ Unit = Unit@();

  <@,@>@ List@ = <@ T@><@ L@>((T@, L@){L@;}, L@){L@;};

  <@ T@>(List@<T@>){Unit@;} S = <@ T@>(List@<T@> mkList) {
    Unit;
  };

  S<Unit@>([Unit]);
}
