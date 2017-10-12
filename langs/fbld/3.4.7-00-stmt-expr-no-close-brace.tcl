set prg {
  MainI.fbld {
    mtype MainI {
      struct Unit();
      func main( ; Unit);
    };
  }

  MainM.fbld {
    mdefn MainM(MainI) {
      struct Unit();

      func main( ; Unit) {
        Unit();
      ; # Missing the '}' brace.
    };
  }
}
fbld-check-error $prg MainM MainM.fbld:7:7
