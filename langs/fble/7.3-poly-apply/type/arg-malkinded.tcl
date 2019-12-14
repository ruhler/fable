fble-test-error 7:11 {
  @ Unit@ = *();

  <<@>@>@ MaybeM@ = <<@>@ M@> { +(M@<Unit@> just, Unit@ nothing); };

  # The poly argument to MaybeM@ has type @, when type <@>@ is expected.
  MaybeM@<Unit@>(nothing: Unit@());
}
