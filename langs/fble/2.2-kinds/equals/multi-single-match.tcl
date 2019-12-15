fble-test {
  @ Unit@ = *();

  # The kinds <@, @><@> and <@><@>@ are equivalent.
  <<@, @>@>@ Id@ = <<@, @>@ A@> { A@<Unit@, Unit@>; };
  <<@><@>@ B@> { Id@<B@>; };
}
