set prg {
  MainI.fbld {
    mtype MainI {
      struct Unit();
      struct A(Unit x, Unit y);
      struct B(A a, A b);

      func main( ; B);
    };
  }

  MainM.fbld {
    mdefn MainM(MainI) {
      struct Unit();
      struct A(Unit x, Unit y);
      struct B(A a, A b);

      func main( ; B) {
        # The variable 'v' is defined twice.
        Unit v = Unit();
        A v = A(v, v);
        B(v, v);
      };
    };
  }
}

fbld-check-error $prg MainM MainM.fbld:10:11
