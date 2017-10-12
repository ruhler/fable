set prg {
  MainI.fbld {
    mtype MainI {
      struct Unit();
      struct A(Unit x, Unit y);
      struct A2(A x, A y);
      struct B(Unit w, A2 a, Unit x);
      func main( ; B);
    };
  }

  MainM.fbld {
    mdefn MainM(MainI) {
      struct Unit();
      struct A(Unit x, Unit y);
      struct A2(A x, A y);
      struct B(Unit w, A2 a, Unit x);
      func main( ; B) {
        # Test a nested let statement.
        Unit u = Unit();
        B(u, { A a = A(u, u); A2(a,a); }, u);
      };
    };
  }
}

fbld-test $prg "main@MainM" {} {
  return B@MainM(Unit@MainM(),A2@MainM(A@MainM(Unit@MainM(),Unit@MainM()),A@MainM(Unit@MainM(),Unit@MainM())),Unit@MainM())
}
