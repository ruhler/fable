" Vim ftplugin file
" Language:	Fble
" Maintainer:	Richard Uhler <ruhler@degralder.com>

if exists("b:did_ftplugin")
  finish
endif
let b:did_ftplugin = 1

setlocal comments=:#
setlocal formatoptions=croqtl

" Add support for 'gf' to jump to a file given a module path.
setlocal isfname=@,48-57,/,.,-,_,+,,,#,$,~,=
setlocal includeexpr=v:fname[1:].'.fble'

