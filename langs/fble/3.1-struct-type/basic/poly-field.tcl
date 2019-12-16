fble-test {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  <@>@ Maybe@ = <@ T@> { +(T@ just, Unit@ nothing); };

  # A struct type with a poly field.
  @ Lib@ = *(Maybe@ nothing, Unit@ blah);
  Lib@ lib = Lib@(<@ T@> { Maybe@<T@>(nothing: Unit@()); }, Unit@());

  Maybe@<Unit@> noUnit = lib.nothing<Unit@>;
  Maybe@<Bool@> noBool = lib.nothing<Bool@>;

  Unit@ _ = noUnit.nothing;
  Unit@ _ = noBool.nothing;
  Unit@();
}
