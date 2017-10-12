set prg {
  MainI.fbld {
    mtype MainI {
      struct Unit();
      struct Donut();
      struct BadStruct(Unit x, Donut x);  # duplicate fields named 'x'
      func main( ; BadStruct);
    };
  }

  MainM.fbld {
    mdefn MainM(MainI) {
      struct Unit();
      struct Donut();
      struct BadStruct(Unit x, Donut x);

      func main( ; BadStruct) {
        BadStruct(Unit(), Donut());
      };
    };
  }
}

fbld-check-error $prg MainM MainI.fbld:5:38


# Even if the types are the same.
set prg {
  MainI.fbld {
    mtype MainI {
      struct Unit();
      struct BadStruct(Unit x, Unit x);  # duplicate fields named 'x'
      func main( ; BadStruct);
    };
  }

  MainM.fbld {
    mdefn MainM(MainI) {
      struct Unit();
      struct BadStruct(Unit x, Unit x);

      func main( ; BadStruct) {
        BadStruct(Unit(), Unit());
      };
    };
  }
}

fbld-check-error $prg MainM MainI.fbld:4:37
