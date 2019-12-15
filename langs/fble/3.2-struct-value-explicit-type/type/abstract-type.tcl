fble-test-error 9:5 {
  @ Unit@ = *();

  <@>@ Id@ = <@ T@> { T@; };

  <@ A@> {
    # There's no way to know if A@ is a struct type, so this should not be
    # allowed.
    Id@<A@>(Unit@());
  };
}
