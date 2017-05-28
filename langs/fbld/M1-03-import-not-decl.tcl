# An import declaration should not count as the definition of a type.
set prg {
  Bool.mdecl {
    mdecl Bool() {
      struct Unit();
      union Bool(Unit True, Unit False);
      func true( ; Bool);
    };
  }

  Bool.mdefn {
    mdefn Bool() {
      struct Unit();
      union Bool(Unit True, Unit False);
      func true( ; Bool) Bool:True(Unit());
    };
  }

  Main.mdecl {
    mdecl Main(Bool) {
      func main( ; Bool@Bool);
    };
  }

  Main.mdefn {
    mdefn Main(Bool) {
      import Bool(true);

      # The Bool type has not been brought in scope, even though there is an import
      # statement for the module Bool.
      func main( ; Bool) {
        true();
      };
    };
  }
}

fbld-check-error $prg Main Main.mdefn:7:20
