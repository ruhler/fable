fble-test {
  # We can import from a child module without the child's parent module being
  # loading.
  { /Bool/Unit%; @(Unit); };

  Unit;
} {
  Bool {
    # This compiler error shouldn't effect the overall program.
    zzz;
  } {
    Unit {
      @ Unit@ = *();
      Unit@ Unit = Unit@();
      @(Unit@, Unit);
    }
  }
}
