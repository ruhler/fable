# Test that we can use a 'using' statement to refer to foreign entities.
set prg {
  UnitM.mtype {
    mtype UnitM<> {
      struct Unit();
    };
  }

  UnitM.mdefn {
    mdefn UnitM< ; ; UnitM<>> {
      struct Unit();
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
      using UnitM<;> { Unit; };
      func main( ; Unit) {
        Unit();
      };
    };
  }
}

fbld-test $prg "main@Main<;>" {} {
  return Unit@UnitM<;>()
}
