" Vim syntax file
" Language:	Fbld
" Maintainer:	Richard Uhler <ruhler@degralder.com>

syn case match
syn keyword	fblcStructure	type struct union func proc
syn keyword	fblcStructure	priv abst 
syn keyword	fblcStructure	module interf
syn keyword fblcInclude import
syn match fblcComment "#.*"

hi def link fblcStructure	Structure
hi def link fblcInclude	Include
hi def link fblcComment	Comment
hi def link fblcId Identifier
