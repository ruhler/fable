fble-test-error 3:22 {
  # The body of the polymorphic type doesn't compile.
  <@>@ M@ = <@ T@> { zzz@; };

  @ Unit@ = *();
  @ MaybeT@ = +(Unit@ nothing, M@<Unit@> just);
  MaybeT@(nothing: Unit@());
}
