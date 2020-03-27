fble-test-error 10:10 {
  @ Unit@ = *();

  <@>@ A@ = <@ T@> { *(); };
  <<@>@>@ B@ = <<@>@ T@> { *(); };

  # The poly types A@ and B@ are not the same, because their argument types
  # have different kinds.
  A@ a = <@ T@> { *()(); };
  B@ b = a;
  Unit@();
}
