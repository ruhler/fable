set prg {
  priv struct Unit();

  module M {
    import @ { Unit; };

    # The public entity main@M must not refer to private entity Unit.
    func main( ; Unit) {
      Unit();
    };
  };
}

fbld-check-error $prg 8:18
