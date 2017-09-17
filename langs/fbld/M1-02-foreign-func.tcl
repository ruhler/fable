# Test that we can call a foreign function.
set prg {
  UnitM.mtype {
    mtype UnitM<> {
      struct Unit();
      func f( ; Unit);
    };
  }

  UnitM.mdefn {
    mdefn UnitM< ; ; UnitM<>> {
      struct Unit();
      func f( ; Unit) {
        Unit();
      };
    };
  }

  Main.mtype {
    mtype Main<> {
      using UnitM<;> { Unit; };
      func main( ; Unit);
    };
  }

  Main.mdefn {
    mdefn Main< ; ; Main<>> {
      using UnitM<;> { Unit; f; };
      func main( ; Unit) {
        f();
      };
    };
  }
}

fbld-test $prg "main@Main<;>" {} {
  return Unit@UnitM<;>()
}
