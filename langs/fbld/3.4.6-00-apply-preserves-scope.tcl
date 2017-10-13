set prg {
  MainI.fbld {
    interf MainI {
      struct Unit();
      struct Donut();
      struct A(Unit x, Donut y);
      struct B(Unit w, A a, Unit z);
      func foo(Unit v ; A);
      func main( ; B);
    };
  }

  MainM.fbld {
    module MainM(MainI) {
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

fbld-test $prg "main@MainM" {} {
  return B@MainM(Unit@MainM(),A@MainM(Unit@MainM(),Donut@MainM()),Unit@MainM())
}
