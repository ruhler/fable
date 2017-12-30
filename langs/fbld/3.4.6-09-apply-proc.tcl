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

      proc myproc( ; Unit x ; Unit) {
        $(x);
      };

      func main( ; Unit) {
        # application cannot be done on a process.
        myproc(Unit());
      };
    };
  }
}

fbld-check-error $prg MainM MainM.fbld:11:9
