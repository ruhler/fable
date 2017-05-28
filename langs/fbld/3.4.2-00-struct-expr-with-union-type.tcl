# A struct expression can't be used for a union type.
set prg {
  Main.mdecl {
    mdecl Main() {
      struct Unit();
      union Bool(Unit True, Unit False);
      func main( ; Bool);
    };
  }

  Main.mdefn {
    mdefn Main() {
      struct Unit();
      union Bool(Unit True, Unit False);

      func main( ; Bool) {
        Bool(Unit(), Unit());
      };
    };
  }
}

fbld-check-error $prg Main Main.mdefn:7:9
