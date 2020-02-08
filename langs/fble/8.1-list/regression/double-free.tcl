fble-test {
  # Regression test that triggered a double free in the compiler at one point.
  @ Unit@ = *();
  Unit@ Unit = Unit@();

  <@>@ P@ = <@ T@> { *(T@ head, S@<T@> tail); },
  <@>@ S@ = <@ T@> { +(P@<T@> cons, Unit@ nil); };

  <@ T@>(T@, S@<T@>){S@<T@>;} ConsS = <@ T@>(T@ a, S@<T@> l) {
    S@<T@>(cons: P@<T@>(a, l));
  };

  <@,@>@ List@ = <@ T@><@ L@>((T@, L@){L@;}, L@){L@;};

  <@ T@>(List@<T@>){S@<T@>;} S = <@ T@>(List@<T@> mkList) {
    mkList<S@<T@>>(ConsS<T@>, S@<T@>(nil: Unit));
  };

  S<Unit@>([Unit@()]);
}
