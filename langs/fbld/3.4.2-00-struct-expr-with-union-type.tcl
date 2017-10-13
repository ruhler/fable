# A struct expression can't be used for a union type.
set prg {
  MainI.fbld {
    interf MainI {
      struct Unit();
      union Bool(Unit True, Unit False);
      func main( ; Bool);
    };
  }

  MainM.fbld {
    module MainM(MainI) {
      struct Unit();
      union Bool(Unit True, Unit False);

      func main( ; Bool) {
        Bool(Unit(), Unit());
      };
    };
  }
}

fbld-check-error $prg MainM MainM.fbld:7:9
