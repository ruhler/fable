fble-test {
  # It should be possible to define mutually recursive types using lets.
  # And in particular, we should be able to collect the memory for those types
  # appropriately if we don't always hold strong references to them.
  { @ A@ = *(B@ b), @ B@ = *(C@ c), @ C@ = *(A@ a); A@; } x = { @ A@ = *(B@ b), @ B@ = *(C@ c), @ C@ = *(A@ a); A@; }(y),
  { @ A@ = *(B@ b), @ B@ = *(C@ c), @ C@ = *(A@ a); B@; } y = { @ A@ = *(B@ b), @ B@ = *(C@ c), @ C@ = *(A@ a); B@; }(z),
  { @ A@ = *(B@ b), @ B@ = *(C@ c), @ C@ = *(A@ a); C@; } z = { @ A@ = *(B@ b), @ B@ = *(C@ c), @ C@ = *(A@ a); C@; }(x);
  x.b.c.a.b.c;
}
