// fblc-snake
//   A program to run fblc programs with a snake interface.

#include <curses.h>   // for initscr, cbreak, etc.
#include <time.h>     // for ctime
#include <sys/time.h> // for gettimeofday

#define MAX_ROW 20
#define MAX_COL 60

typedef int Time;

void GetCurrentTime(Time* time)
{
  struct timeval tv;
  gettimeofday(&tv, NULL);
  *time = 1000 * tv.tv_sec + tv.tv_usec/1000;
}

void AddTimeMillis(Time* time, int millis)
{
  *time += millis;
}

int DiffTimeMillis(Time* a, Time* b)
{
  return *a - *b;
}

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

  Time tnext;
  GetCurrentTime(&tnext);
  AddTimeMillis(&tnext, 1000);
  int ticks = 0;
  while (true) {
    Time tnow;
    GetCurrentTime(&tnow);
    int dt = DiffTimeMillis(&tnext, &tnow);
    if (dt <= 0) {
      ticks++;
      AddTimeMillis(&tnext, 1000);
      char buf[32];
      sprintf(buf, "%i", ticks);
      mvaddstr(3, 3, buf);
    } else {
      timeout(dt);
      int c = getch();
      if (c != ERR) {
        mvaddch(2, 3, c);
      }
    }
    refresh();
  }

  endwin();
  return 0;
}
