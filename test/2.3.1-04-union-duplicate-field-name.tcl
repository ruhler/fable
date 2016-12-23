
# Each field of a union type must have a different name.
set prg {
  struct Unit();
  struct Donut();
  union BadStruct(Unit x, Donut x);

  func main( ; BadStruct) {
    BadStruct:x(Unit());
  };
}
fblc-check-error $prg


# Even if the types are the same.
set prg {
  struct Unit();
  union BadStruct(Unit x, Unit x);

  func main( ; BadStruct) {
    BadStruct:x(Unit());
  };
}
fblc-check-error $prg
