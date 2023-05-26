" Vim ftplugin file
" Language:	Fble
" Maintainer:	Richard Uhler <ruhler@degralder.com>

if exists("b:did_ftplugin")
  finish
endif
let b:did_ftplugin = 1

setlocal comments=:#
setlocal formatoptions=croqtl

" Include '%', '/' in keywords so that a module path is considered a word.
setlocal iskeyword=@,48-57,%,/,_,192-255

" Add support for 'gf' to jump to a file given a module path.
setlocal isfname=@,48-57,/,.,-,_,+,,,#,$,~,=
setlocal includeexpr=v:fname[1:].'.fble'

