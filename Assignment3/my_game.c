#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#ifdef _WIN32
  #include <conio.h>
  #include <windows.h>
  #define msleep(x) Sleep(x)
  static int kb_hit(){ return _kbhit(); }
  static char kb_get(){ return _getch(); }
#else
  #include <termios.h>
  #include <unistd.h>
  #include <sys/select.h>
  static struct termios oldt;
  static void enable_raw(){
    struct termios t;
    tcgetattr(0,&oldt);
    tcgetattr(0,&t);
    t.c_lflag &= ~(ICANON|ECHO);
    t.c_cc[VMIN] = 0; t.c_cc[VTIME] = 0;
    tcsetattr(0,TCSANOW,&t);
  }
  static void disable_raw(){ tcsetattr(0,TCSANOW,&oldt); }
  static int kb_hit(){
    fd_set rf; struct timeval tv={0,0};
    FD_ZERO(&rf); FD_SET(0,&rf);
    return select(1,&rf,NULL,NULL,&tv) == 1;
  }
  static char kb_get(){ char c=0; if(read(0,&c,1)==1) return c; return 0; }
  static void msleep(int ms){ usleep(ms*1000); }
#endif

#define W 21
#define H 15
#define MAX_EN 12

char grid[H][W];
int px,py, exs[MAX_EN], eys[MAX_EN], en;
int gold_need, gold_have, score, timer_moves;
#ifdef _WIN32
static HANDLE hOut = NULL;
static void ensure_handle(){ if(!hOut) hOut = GetStdHandle(STD_OUTPUT_HANDLE); }
static void cursor_home(){
  ensure_handle();
  COORD pos = {0,0};
  SetConsoleCursorPosition(hOut,pos);
}
static void hide_cursor(){
  ensure_handle();
  CONSOLE_CURSOR_INFO info;
  GetConsoleCursorInfo(hOut,&info);
  info.bVisible = FALSE;
  SetConsoleCursorInfo(hOut,&info);
}
static void show_cursor(){
  ensure_handle();
  CONSOLE_CURSOR_INFO info;
  GetConsoleCursorInfo(hOut,&info);
  info.bVisible = TRUE;
  SetConsoleCursorInfo(hOut,&info);
}
#else
static void cursor_home(){ printf("\033[H"); }
static void hide_cursor(){ printf("\033[?25l"); fflush(stdout); }
static void show_cursor(){ printf("\033[?25h"); fflush(stdout); }
#endif

void clear_screen_once(){
#ifdef _WIN32
  system("cls");
#else
  printf("\033[2J");
#endif
}
void fill_empty(){
  for(int r=0;r<H;r++) for(int c=0;c<W;c++)
    grid[r][c] = (r==0||c==0||r==H-1||c==W-1) ? '#' : ' ';
}
int empty_cell(int r,int c){ return r>0 && c>0 && r<H-1 && c<W-1 && grid[r][c] != '#'; }
void scatter(int n){
  for(int i=0;i<n;i++){ int r=1+rand()%(H-2), c=1+rand()%(W-2); grid[r][c] = '#'; }
}
void place_items(int lvl){
  gold_need = 3 + lvl;
  gold_have = 0;

  int traps = 2 + lvl/2;
  en = (lvl <= 1) ? 1 : (lvl <= 5 ? lvl : 5 + (lvl - 5) / 2);
  if (en > MAX_EN) en = MAX_EN;

  int empties[H * W];
  int ec = 0;
  for(int r=1;r<=H-2;r++){
    for(int c=1;c<=W-2;c++){
      if(grid[r][c] == ' ') empties[ec++] = (r << 8) | c;
    }
  }
  int needed = gold_need + traps + en;
  if(ec < needed){
    if(ec <= 0){
      gold_need = 0; traps = 0; en = 0;
    } else {
      if(ec < gold_need){
        gold_need = ec; traps = 0; en = 0;
      } else {
        int remain = ec - gold_need;
        if(remain < traps){ traps = remain; en = 0; }
        else { remain -= traps; if(remain < en) en = remain; }
      }
    }
  }
  for(int i = ec - 1; i > 0; --i){
    int j = rand() % (i + 1);
    int tmp = empties[i]; empties[i] = empties[j]; empties[j] = tmp;
  }

  int idx = 0;
  for(int i=0;i<gold_need && idx < ec; i++, idx++){
    int packed = empties[idx]; int r = packed >> 8; int c = packed & 0xFF;
    grid[r][c] = '$';
  }
  for(int i=0;i<traps && idx < ec; i++, idx++){
    int packed = empties[idx]; int r = packed >> 8; int c = packed & 0xFF;
    grid[r][c] = 'X';
  }
  for(int i=0;i<en && idx < ec; i++, idx++){
    int packed = empties[idx]; int r = packed >> 8; int c = packed & 0xFF;
    eys[i] = r; exs[i] = c; grid[r][c] = 'M';
  }

  timer_moves = 80 - lvl * 6;
  if(timer_moves < 30) timer_moves = 30;
}

void init_level(int lvl){
  fill_empty();
  scatter(28 + lvl * 4);
  px = 1; py = 1; grid[py][px] = '@';
  place_items(lvl);
}

void draw(int lvl){
  cursor_home();
  printf("Level %d  Score %d  Gold %d/%d  Time %d  Enemies %d\n",
         lvl, score, gold_have, gold_need, timer_moves, en);
  for(int r=0;r<H;r++){
    for(int c=0;c<W;c++) putchar(grid[r][c]);
    putchar('\n');
  }
  printf("Move: W A S D   (q to quit)\n");
}

void move_enemies(){
  for(int i=0;i<en;i++){
    int r = eys[i], c = exs[i];
    if(grid[r][c] == 'M') grid[r][c] = ' ';
    int d = rand() % 4, nr = r, nc = c;
    if(d==0) nr--; else if(d==1) nr++; else if(d==2) nc--; else nc++;
    if(empty_cell(nr,nc) && grid[nr][nc] != 'M'){ eys[i]=nr; exs[i]=nc; }
    else {
      for(int t=0;t<4;t++){
        d = rand()%4; nr=r; nc=c;
        if(d==0) nr--; else if(d==1) nr++; else if(d==2) nc--; else nc++;
        if(empty_cell(nr,nc) && grid[nr][nc] != 'M'){ eys[i]=nr; exs[i]=nc; break; }
      }
    }
  }
  for(int i=0;i<en;i++){
    int r = eys[i], c = exs[i];
    grid[r][c] = 'M';
  }
}

int load_high(){ FILE *f = fopen("treasure_high.txt","r"); int h = 0; if(f){ fscanf(f,"%d",&h); fclose(f); } return h; }
void save_high(int v){ FILE *f = fopen("treasure_high.txt","w"); if(f){ fprintf(f,"%d\n",v); fclose(f); } }

int main(){
  srand((unsigned)time(NULL));
  int level = 1;
  int high = load_high();

#ifndef _WIN32
  enable_raw();
#endif

  clear_screen_once();
#ifdef _WIN32
  hide_cursor();
#else
  hide_cursor();
#endif

  init_level(level);
  cursor_home();
  int tick = 0;
  const int TMS = 90;

  while(1){
    draw(level);

    int waited = 0; char in = 0;
    while(waited < TMS){
      if(kb_hit()){ in = kb_get(); break; }
      msleep(10); waited += 10;
    }

    if(in){
      if(in >= 'A' && in <= 'Z') in = in - 'A' + 'a';
      if(in == 'q') break;
      int nx = px, ny = py;
      if(in == 'w') ny--; else if(in == 's') ny++; else if(in == 'a') nx--; else if(in == 'd') nx++;
      if(!(nx < 1 || ny < 1 || nx > W-2 || ny > H-2) && grid[ny][nx] != '#'){
        grid[py][px] = ' ';
        px = nx; py = ny;
        if(grid[py][px] == '$'){ gold_have++; score += 10; }
        if(grid[py][px] == 'X'){ show_cursor(); printf("Stepped on TRAP! GAME OVER\n"); goto END; }
        if(grid[py][px] == 'M'){ show_cursor(); printf("Caught by ENEMY! GAME OVER\n"); goto END; }
        grid[py][px] = '@';
      }
    }

    move_enemies();

    for(int i=0;i<en;i++){
      if(exs[i] == px && eys[i] == py){ show_cursor(); printf("Enemy hit you! GAME OVER\n"); goto END; }
    }

    tick++; if(tick % 3 == 0) timer_moves--;
    if(timer_moves <= 0){ show_cursor(); printf("Time up! GAME OVER\n"); break; }

    if(gold_have >= gold_need){
      score += 50;
      level++;
      for(int r=0;r<H;r++) for(int c=0;c<W;c++) if(grid[r][c] == 'M') grid[r][c] = ' ';
      init_level(level);
    }
  }
END:
  printf("Final Score: %d\n", score);
  if(score > high){ printf("New High Score!\n"); save_high(score); } else printf("High Score: %d\n", high);

  show_cursor();
#ifndef _WIN32
  disable_raw();
#endif
return 0;
}
