fble-test {
  @ Unit@ = *();

  # Use of a polymorphic type variable.
  <<@>@>@ Funny@ = <<@>@ M@> { *(M@<Unit@> funny); };
  <@>@ Maybe@ = <@ T@> { +(T@ just, Unit@ nothing); };

  Funny@<Maybe@> funny = Funny@<Maybe@>(Maybe@<Unit@>(just: Unit@()));
  funny.funny.just;
}
