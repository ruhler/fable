fble-test-error 9:5 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  @ Pair@ = *(Bool@ x, Bool@ y);
  @ Maybe@ = +(Pair@ just, Unit@ nothing);
  Maybe@ m = Maybe@(nothing: Unit@());

  # Struct eval is strict in the struct object.
  m.just {
    x.true;
  };
}
