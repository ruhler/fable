# Library of tags to use in @usage docs.

# @GenericProgramInfo
# A description of generic program information options.
proc block_GenericProgramInfo {} {
  ::block_subsection "Generic Program Information" {
@opt[@l[-h], @l[--help]]
 display this help text and exit

@opt[@l[-v], @l[--version]]
 display version information and exit
}
}

# @ModuleInput
# A description of module input options.
proc block_ModuleInput {} {
  ::block_subsection "Module Input" {
@opt[@l[-I] @a[DIR]]
 add @a[DIR] to the module search path

@opt[@l[-p], @l[--package] @a[PACKAGE]]
 add @a[PACKAGE] to the module search path

@opt[@l[-m], @l[--module] @a[MODULE_PATH]]
 the module path of the input module

Packages are searched for in the package search path specified by the
@l[FBLE_PACKAGE_PATH] environment variable followed by the system default
package search path. Modules are searched for in the module search path.

For example, if @l[FBLE_PACKAGE_PATH] is @l[/fble/pkgs] and you provide
command line options @l[-p core -m /Core/Unit%], the @l[-p core] option adds
@l[/fble/pkgs/core] to the module search path, and the @l[-m /Core/Unit%]
option will look for the module at @l[/fble/pkgs/core/Core/Unit.fble].
}
}

