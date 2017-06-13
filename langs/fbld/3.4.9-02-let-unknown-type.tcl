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
        # The type 'Foo' has not been declared.
        Foo v = Unit();
        Unit();
      };
    };
  }
}

fbld-check-error $prg Main Main.mdefn:7:9
