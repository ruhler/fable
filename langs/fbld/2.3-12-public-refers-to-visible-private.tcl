set prg {
  priv struct PrivUnit();

  priv module M {
    import @ { PrivUnit; };

    # The public entity pub can refer to private entity PrivUnit, because the
    # module M has no more visibility than the PrivUnit type.
    func pub( ; PrivUnit) {
      PrivUnit();
    };
  };

  struct Unit();
  func main( ; Unit) {
    Unit();
  };
}

fbld-test $prg "main" {} {
  return Unit()
}
