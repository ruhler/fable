" Vim syntax file
" Language:	Fbld
" Maintainer:	Richard Uhler <ruhler@degralder.com>

syn case match
syn keyword	fblcStructure	type struct union func proc mtype mdefn mdecl
syn keyword	fblcStructure	alias define let local module
syn keyword fblcInclude import using
syn region fblcId start="[[]" end="[]]"
syn match fblcComment "#.*"

hi def link fblcStructure	Structure
hi def link fblcInclude	Include
hi def link fblcComment	Comment
hi def link fblcId Identifier
