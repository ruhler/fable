set prg {
  Main.mtype {
    mtype Main {
      struct Unit();
      type Recursive;
      func main( ; Unit);
    };
  }

  Main.mdefn {
    mdefn Main(Main) {
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

fbld-test $prg "main@Main" {} {
  return Unit@Main()
}
