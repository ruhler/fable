fble-test {
  # Test a type let in a type context, as opposed to an expression context.
  @ Bool@ = {
    @ Unit@ = *();
    +(Unit@ true, Unit@ false);
  };
  Bool@(true: *()()).true;
}
