" Vim syntax file
" Language:	Fble
" Maintainer:	Richard Uhler <ruhler@degralder.com>

syn match fbleComment "#.*"
syn match fbleLabel "\w\+:"
syn match fbleType "\w\+@"

hi def link fbleComment	Comment
hi def link fbleLabel	Label
hi def link fbleType	Type
