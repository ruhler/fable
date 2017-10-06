set prg {
  Main.mtype {
    mtype Main {
      struct Unit();
      struct A(Unit x, Unit y);
      struct A2(A x, A y);
      struct B(Unit w, A2 a, Unit x);
      func main( ; B);
    };
  }

  Main.mdefn {
    mdefn Main(Main) {
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

fbld-test $prg "main@Main" {} {
  return B@Main(Unit@Main(),A2@Main(A@Main(Unit@Main(),Unit@Main()),A@Main(Unit@Main(),Unit@Main())),Unit@Main())
}
