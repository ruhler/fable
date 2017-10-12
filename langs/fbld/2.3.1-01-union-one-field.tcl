set prg {
  MainI.fbld {
    mtype MainI {
      struct Unit();
      union OneField(Unit x);
      func main( ; OneField);
    };
  }

  MainM.fbld {
    mdefn MainM(MainI) {
      struct Unit();
      union OneField(Unit x);

      func main( ; OneField) {
        OneField:x(Unit());
      };
    };
  }
}

fbld-test $prg "main@MainM" {} {
  return OneField@MainM:x(Unit@MainM())
}
