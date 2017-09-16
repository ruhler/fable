set prg {
  Main.mtype {
    mtype Main<> {
      struct Unit();
      func main( ; Unit);
    };
  }

  Main.mdefn {
    mdefn Main< ; ; Main<>> {
      struct Unit();

      func main( ; Unit) {
        Unit();
      ; # Missing the '}' brace.
    };
  }
}
fbld-check-error $prg Main Main.mdefn:7:7
