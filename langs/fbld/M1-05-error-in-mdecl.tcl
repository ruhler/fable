set prg {
  Main.mdecl {
    mdecl Main() {
      # Unit is not declared in the mdecl, even though it is properly declared
      # in the mdefn.
      func main( ; Unit);
    };
  }

  Main.mdefn {
    mdefn Main() {
      struct Unit();

      func main( ; Unit) {
        Unit();
      };
    };
  }
}

skip fbld-check-error $prg Main Main.mdecl:5:20
