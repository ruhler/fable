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

      # Function with no input arguments.
      func main( ; Unit) {
        Unit();
      };
    };
  }
}

fbld-test $prg "main@MainM" {} {
  return Unit@MainM()
}
