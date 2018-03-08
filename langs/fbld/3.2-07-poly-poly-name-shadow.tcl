set prg {
  struct Unit();

  # Can't have a nested static parameter that shadows an existing parameter.
  struct Pair<type A, struct S<type A>(A x)>(A first, S<Unit> second);
}

fbld-check-error $prg 5:37
