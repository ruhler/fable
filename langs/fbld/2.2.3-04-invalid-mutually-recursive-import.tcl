set prg {
  # Mutually recursive import not allowed.
  import A { B; };
  import B { A; };
}

fbld-check-error $prg 3:14
