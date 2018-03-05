set prg {
  struct Unit();

  interf I {
    import @ { Unit; };
    func f( ; Unit);
  };

  func main<module M(I)>( ; Unit) {
    # The function g is not found in the interface for module M.
    g@M();
  };
}

fbld-check-error $prg 11:5
