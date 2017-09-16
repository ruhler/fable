# A basic test case using multiple modules.
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
      func main( ; Unit@UnitM<;>);
    };
  }

  Main.mdefn {
    mdefn Main< ; ; Main<>> {
      func main( ; Unit@UnitM<;>) {
        Unit@UnitM<;>();
      };
    };
  }
}

skip fbld-test $prg "main@Main<;>" {} {
  return Unit@UnitM<;>()
}
