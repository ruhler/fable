fble-test-error 8:20 {
  @ Unit@ = *();
  \<@; @> Maybe@ = \<@ T@> { +(T@ just, Unit@ nothing); };

  # The value of the polymorphic entity is a poly expression taking two poly
  # args, but the declared type of Maybe@ is a poly expression taking one poly
  # arg.
  Maybe@ Nothing = \<@ T@, @ B@> { Maybe@<T@>(nothing: Unit@()); };
  Nothing<Unit@>;
}
