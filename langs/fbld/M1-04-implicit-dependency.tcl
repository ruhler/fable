set prg {
  Unit.mdecl {
    mdecl Unit() {
      struct Unit();
    };
  }

  Unit.mdefn {
    mdefn Unit() {
      struct Unit();
    };
  }

  Bool.mdecl {
    mdecl Bool(Unit) {
      union Bool(Unit@Unit true, Unit@Unit false);
    };
  }

  Bool.mdefn {
    mdefn Bool(Unit) {
      union Bool(Unit@Unit true, Unit@Unit false);
    };
  }

  BoolUtil.mdecl {
    mdecl BoolUtil(Bool, Unit) {
      func True( ; Bool@Bool);
      func Ignore(Bool@Bool x ; Unit@Unit);
    };
  }

  BoolUtil.mdefn {
    mdefn BoolUtil(Bool, Unit) {
      func True( ; Bool@Bool) Bool@Bool:true(Unit@Unit());
      func Ignore(Bool@Bool x ; Unit@Unit) Unit@Unit();
    };
  }

  Main.mdecl {
    mdecl Main(Unit) {
      func Main( ; Unit@Unit);
    };
  }

  Main.mdefn {
    mdefn Main(Unit, BoolUtil) {
      func main( ; Unit@Unit) {
        # The Main module should list Bool as a dependency because the result
        # of calling True is a Bool, even though main doesn't explicitly refer
        # to a named entity from the Bool module.
        Ignore@BoolUtil(True@BoolUtil());
      };
    };
  }
}

skip fbld-check-error $prg Main Main.mdefn:7:25
