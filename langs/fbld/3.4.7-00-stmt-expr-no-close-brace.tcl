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
        Unit();
      ; # Missing the '}' brace.
    };
  }
}
fbld-check-error $prg Main Main.mdefn:7:7
