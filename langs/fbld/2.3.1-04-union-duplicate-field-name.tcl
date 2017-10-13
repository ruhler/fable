set prg {
  MainI.fbld {
    interf MainI {
    };
  }

  MainM.fbld {
    module MainM(MainI) {
      # Each field of a union type must have a different name.
      struct Unit();
      struct Donut();
      union BadStruct(Unit x, Donut x);

      func main( ; BadStruct) {
        BadStruct:x(Unit());
      };
    };
  }
}

fbld-check-error $prg MainM MainM.fbld:6:37
