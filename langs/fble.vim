" Vim syntax file
" Language:	Fble
" Maintainer:	Richard Uhler <ruhler@degralder.com>

syn match Comment "#.*"
syn match Label "\w\+:"
syn match Type "\w\+@"
syn match Include "\S\+%"
syn match String "|\w\+"
syn region String start=+'+ skip=+''+ end=+'+

