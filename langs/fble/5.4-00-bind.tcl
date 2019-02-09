fble-test {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());
  Bool@ false = Bool@(false: Unit@());

  @ Maybe@ = +(Bool@ just, Unit@ nothing);

  \(Maybe@ m; \(\(Bool@ x; Bool@) f; Maybe@)) Map = \(Maybe@ m) {
    \(\(Bool@ x; Bool@) f) {
      ?(m;
         just: Maybe@(just: f(m.just)),
         nothing: Maybe@(nothing: Unit@()));
     };
  };

  Maybe@ mtrue = Maybe@(just: true);

  Maybe@ mfalse = {
    Bool@ x <- Map(mtrue);
    ?(x; true: false, false: true);
  };

  mfalse.just.false;
}
