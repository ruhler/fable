set prg {
  MainI.fbld {
    interf MainI {
      struct Unit();
      func main( ; Unit);
    };
  }

  MainM.fbld {
    module MainM(MainI) {
      struct Unit();

      func main( ; Unit) {
        Unit();
      ; # Missing the '}' brace.
    };
  }
}
fbld-check-error $prg MainM MainM.fbld:7:7
