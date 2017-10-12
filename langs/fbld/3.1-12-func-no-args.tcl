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
