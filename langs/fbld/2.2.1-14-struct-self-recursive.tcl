set prg {
  MainI.fbld {
    mtype MainI {
      struct Unit();
      type Recursive;
      func main( ; Unit);
    };
  }

  MainM.fbld {
    mdefn MainM(MainI) {
      struct Unit();

      # In theory a struct can have a field with its same type, though it's not
      # clear how you could possibly construct such a thing.
      struct Recursive(Recursive x, Recursive y);

      func main( ; Unit) {
        Unit();
      };
    };
  }
}

fbld-test $prg "main@MainM" {} {
  return Unit@MainM()
}
