set prg {
  Main.mdecl {
    mdecl Main() {
      struct Unit();
      struct Donut();
      struct A(Unit x, Donut y);
      struct B(Unit w, A a, Unit z);
      func foo(Unit v ; A);
      func main( ; B);
    };
  }

  Main.mdefn {
    mdefn Main() {
      # Test that we can still refer to variables in scope after a function call.
      struct Unit();
      struct Donut();
      struct A(Unit x, Donut y);
      struct B(Unit w, A a, Unit z);

      func foo(Unit v ; A) {
        A(v, Donut());
      };

      func main( ; B) {
        Unit v = Unit();
        B(v, foo(v), v);
      };
    };
  }
}

fbld-test $prg main@Main {} {
  return B@Main(Unit@Main(),A@Main(Unit@Main(),Donut@Main()),Unit@Main())
}
