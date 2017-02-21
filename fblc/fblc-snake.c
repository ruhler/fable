// fblc-snake
//   A program to run fblc programs with a snake interface.

#include <curses.h>   // for initscr, cbreak, etc.

#define MAX_ROW 20
#define MAX_COL 60

int main(int argc, char* argv[])
{
  initscr();
  cbreak();
  noecho();

  for (int c = 0; c <= MAX_COL + 2; ++c) {
    mvaddch(0, c, '#');
    mvaddch(MAX_ROW + 2, c, '#');
  }

  for (int r = 1; r <= MAX_ROW + 1; ++r) {
    mvaddch(r, 0, '#');
    mvaddch(r, MAX_COL + 2, '#');
  }
  refresh();

  getch();

  endwin();
  return 0;
}
