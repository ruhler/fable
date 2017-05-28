set prg {
  Main.mdecl {
    mdecl Main() {
      struct Unit();
      func main( ; Unit);
    };
  }

  Main.mdefn {
    mdefn Main() {
      struct Unit();

      func main( ; Unit) {
        # The function 'f' has not been declared.
        f(Unit());
      };
    };
  }
}

fbld-check-error $prg Main Main.mdefn:7:9
