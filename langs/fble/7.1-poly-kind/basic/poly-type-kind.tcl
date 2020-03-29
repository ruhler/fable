fble-test {
  @ Unit@ = *();

  # Test use of a poly type kind spec.
  # The type of Maybe@ will be automatically inferred.
  <@>@ Maybe@ = <@ T@> { +(T@ just, Unit@ nothing); };

  <@ T@> { @<+(T@ just, Unit@ nothing)>; } X@ = Maybe@;
  X@;
}
