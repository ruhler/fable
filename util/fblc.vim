" Vim syntax file
" Language:	Fblc
" Maintainer:	Richard Uhler <ruhler@degralder.com>
" Last Change:	2015 May 10

syn case match
syn keyword	fblcStructure	struct union func
syn match fblcComment "//.*"

hi def link fblcStructure	Structure
hi def link fblcComment	Comment
