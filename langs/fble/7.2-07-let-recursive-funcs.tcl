fble-test {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);

  @ P@ = *(Bool@ head, S@ tail),
  @ S@ = +(Unit@ nil, P@ cons);

  # Regression test for when mutually recursive functions like this led to a
  # memory leak.
  \(S@; S@) CopyS = \(S@ x) {
    ?(x; nil: S@(nil: Unit@()), cons: S@(cons: CopyP[x.cons]));
  },
  \(P@; P@) CopyP = \(P@ x) {
    P@(x.head, CopyS[x.tail]);
  };

  Unit@();
}
