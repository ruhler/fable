" Vim ftplugin file
" Language:	Fbld
" Maintainer:	Richard Uhler <ruhler@degralder.com>

if exists("b:did_ftplugin")
  finish
endif
let b:did_ftplugin = 1

" From the language's point of view, next line arguments are always indented a
" single space. Set shiftwidth to match that, because in the case of fbld, the
" user doesn't really get a choice of preference here.
setlocal shiftwidth=1
