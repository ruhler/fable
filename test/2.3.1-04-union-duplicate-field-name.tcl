set prg {
  // Each field of a union type must have a different name.
  struct Unit();
  struct Donut();
  union BadStruct(Unit x, Donut x);

  func main( ; BadStruct) {
    BadStruct:x(Unit());
  };
}
fblc-check-error $prg 5:33

set prg {
  // Even if the types are the same.
  struct Unit();
  union BadStruct(Unit x, Unit x);

  func main( ; BadStruct) {
    BadStruct:x(Unit());
  };
}
fblc-check-error $prg 4:32
