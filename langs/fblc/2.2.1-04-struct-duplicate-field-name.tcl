
# Each field of a struct type must have a different name.
set prg {
  struct Unit();
  struct Donut();
  struct BadStruct(Unit x, Donut x);

  func main( ; BadStruct) {
    BadStruct(Unit(), Donut());
  };
}
fblc-check-error $prg 4:34


# Even if the types are the same.
set prg {
  struct Unit();
  struct BadStruct(Unit x, Unit x);

  func main( ; BadStruct) {
    BadStruct(Unit(), Unit());
  };
}
fblc-check-error $prg 3:33
