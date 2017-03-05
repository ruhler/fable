set prg {
  # In theory a struct can have a field with its same type, though it's not
  # clear how you could possibly construct such a thing.
  struct Unit();
  struct Recursive(Recursive x, Recursive y);

  func main( ; Unit) {
    Unit();
  };
}

fblc-test $prg main {} "return Unit()"
