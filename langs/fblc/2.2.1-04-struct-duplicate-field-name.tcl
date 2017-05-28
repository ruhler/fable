set prg {
  # Each field of a struct type must have a different name.
  struct Unit();
  struct Donut();
  struct BadStruct(Unit x, Donut x);

  func main( ; BadStruct) {
    BadStruct(Unit(), Donut());
  };
}
fblc-check-error $prg 5:34


# Even if the types are the same.
set prg {
  struct Unit();
  struct BadStruct(Unit x, Unit x);

  func main( ; BadStruct) {
    BadStruct(Unit(), Unit());
  };
}
fblc-check-error $prg 3:33
