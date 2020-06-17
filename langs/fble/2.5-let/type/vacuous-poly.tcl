fble-test-error 5:8 {
  @ Unit@ = *();

  # The poly type F@ must not be vacuously defined like this.
  <@>@ F@ = <@ T@> { F@<T@>; };

  F@<Unit@>();
}
