" Vim syntax file
" Language:	Fble
" Maintainer:	Richard Uhler <ruhler@degralder.com>

" TODO: Using \w for normal characters fails to match many characters.
" Is there some way to factor out the actual regex for a normal character?

syn match Comment "#.*"
syn match Type "\w\+@"
syn match Label "\w*:"
syn match Label "\w\+@:"
syn match Include "\(\w\|[/]\)\+%"
syn match String "|\w\+"
syn region String start=+'+ skip=+''+ end=+'+

