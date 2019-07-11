A Sudoku Solver

Solving Rules:

* Fundamental Constraint:
  Cell A cannot contain a number that with certainty appears in another cell B
  when A and B belong to the same group.
  For example, if there is a 4 in a particular row, no other cell in that row
  can have the value 4.

* If there is only one value that a cell can take on, that cell must take on
  that value.

* If there is only one cell in a group that can take on a certain value, then
  that cell must take on that value.
  For example, if the cell is the only one in a group that has the possibility
  of being 4, then that cell must be 4.

* Generalization of previous point: if there are N cells in a group that can
  only take on one of a collection of N values, then those N cells cannot take
  on any other value. For example, if there are only 2 cells that could take
  on the values 4 or 5, then those 2 cells must take on the values 4 or 5,
  then cannot take on any other value.

* Combinatorial Search: If choice of a cell leads to an inconsistency, then
  the cell cannot take on that chosen value. In other words: guess and check.
  Presumably a last resort, and maybe not at all necessary, for well formed
  puzzles?
  

