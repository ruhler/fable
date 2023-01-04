# Extra fbld tags used in fble.fbld

# @AbstractSyntax[INLINE content]
# Describes the abstract syntax for a language construct.
# @param content  The abstract syntax.
proc block_AbstractSyntax {content} {
  ::block_definition "Abstract Syntax" "@code\[txt\]\[$content\]"
}

# @ConcreteSyntax[INLINE content]
# Describes the concrete syntax for a language construct.
# @param content  The concrete syntax.
proc block_ConcreteSyntax {content} {
  ::block_definition "Concrete Syntax" "@code\[txt\]\[$content\]"
}

# @Example[INLINE content]
# Gives example fble code for a language construct.
# @param content  The example code.
proc block_Example {content} {
  ::block_definition "Example" "@code\[fble\]\[$content\]"
}
