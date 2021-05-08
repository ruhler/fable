fble-test-compile-error 4:15 {
  # You are not allowed to use a variable to refer to a type of a type for
  # polymorphic types.
  <@>@ Foo@ = <@ T@>{ @<T@>; };
  Foo@;
}
