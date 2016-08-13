
# In theory a struct can have a field with its same type, though it's not
# clear how you could possibly construct such a thing.
set prg {
  struct Unit();
  struct Recursive(Recursive x, Recursive y);

  func main( ; Unit) {
    Unit();
  };
}
expect_result Unit() $prg main

