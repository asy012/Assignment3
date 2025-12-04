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
 static void enable_raw(){ struct termios t; tcgetattr(0,&oldt); tcgetattr(0,&t); t.c_lflag&=~(ICANON|ECHO); t.c_cc[VMIN]=0; t.c_cc[VTIME]=0; tcsetattr(0,TCSANOW,&t); }
 static void disable_raw(){ tcsetattr(0,TCSANOW,&oldt); }
 static int kb_hit(){ fd_set rf; struct timeval tv={0,0}; FD_ZERO(&rf); FD_SET(0,&rf); return select(1,&rf,NULL,NULL,&tv)==1; }
 static char kb_get(){ char c=0; if(read(0,&c,1)==1) return c; return 0; }
 static void msleep(int ms){ usleep(ms*1000); }
#endif

#define W 21
#define H 15
#define MAX_EN 10

char grid[H][W];
int px,py, ex[MAX_EN], ey[MAX_EN], en;
int gold_need, gold_have, score, timer_moves;

static void home(){ printf("\033[H"); }
static void hide_cursor(){ printf("\033[?25l"); fflush(stdout); }
static void show_cursor(){ printf("\033[?25h"); fflush(stdout); }

void fill(){ for(int r=0;r<H;r++) for(int c=0;c<W;c++) grid[r][c]=(r==0||c==0||r==H-1||c==W-1)?'#':' '; }
int empty(int r,int c){ return r>0&&c>0&&r<H-1&&c<W-1 && grid[r][c]==' '; }
void scatter(int n){ for(int i=0;i<n;i++){ int r=1+rand()%(H-2), c=1+rand()%(W-2); grid[r][c]='#'; } }

void place(int lvl){
  gold_need = 3+lvl; gold_have=0;
  int traps = 2 + lvl/2;
  en = (lvl<=1)?1:(lvl<=5?lvl:5+(lvl-5)/2); if(en>MAX_EN) en=MAX_EN;
  int empties[H*W], ec=0;
  for(int r=1;r<H-1;r++) for(int c=1;c<W-1;c++) if(grid[r][c]==' ') empties[ec++]=(r<<8)|c;
  int need = gold_need+traps+en;
  if(ec < need){
    if(ec <= 0){ gold_need=traps=en=0; }
    else{
      if(ec < gold_need){ gold_need=ec; traps=en=0; }
      else{ int rem=ec-gold_need; if(rem < traps){ traps=rem; en=0; } else{ rem -= traps; if(rem < en) en=rem; } }
    }
  }
  for(int i=ec-1;i>0;i--){ int j = rand()%(i+1); int t=empties[i]; empties[i]=empties[j]; empties[j]=t; }
  int idx=0;
  for(int i=0;i<gold_need && idx<ec;i++,idx++){ int p=empties[idx], r=p>>8, c=p&0xFF; grid[r][c]='$'; }
  for(int i=0;i<traps && idx<ec;i++,idx++){ int p=empties[idx], r=p>>8, c=p&0xFF; grid[r][c]='X'; }
  for(int i=0;i<en && idx<ec;i++,idx++){ int p=empties[idx], r=p>>8, c=p&0xFF; ey[i]=r; ex[i]=c; grid[r][c]='M'; }
  timer_moves = 80 - lvl*6; if(timer_moves<30) timer_moves=30;
}

void init_level(int lvl){ fill(); scatter(24 + lvl*3); px=1; py=1; grid[py][px]='@'; place(lvl); }

void draw(int lvl){
  home();
  printf("Level %d Score %d Gold %d/%d Time %d Enemies %d\n", lvl, score, gold_have, gold_need, timer_moves, en);
  for(int r=0;r<H;r++){ for(int c=0;c<W;c++) putchar(grid[r][c]); putchar('\n'); }
  printf("Move: W A S D (q quit)\n");
}

void move_enemies(){
  for(int i=0;i<en;i++){
    int r=ey[i], c=ex[i];
    if(grid[r][c]=='M') grid[r][c]=' ';
    int d=rand()%4, nr=r, nc=c;
    if(d==0) nr--; else if(d==1) nr++; else if(d==2) nc--; else nc++;
    if(empty(nr,nc) && grid[nr][nc] != 'M'){ ey[i]=nr; ex[i]=nc; }
    else for(int t=0;t<4;t++){ d=rand()%4; nr=r; nc=c; if(d==0) nr--; else if(d==1) nr++; else if(d==2) nc--; else nc++; if(empty(nr,nc)&&grid[nr][nc]!='M'){ ey[i]=nr; ex[i]=nc; break; } }
  }
  for(int i=0;i<en;i++) grid[ey[i]][ex[i]]='M';
}

int load_high(){ FILE *f=fopen("treasure_high.txt","r"); int h=0; if(f){ fscanf(f,"%d",&h); fclose(f);} return h; }
void save_high(int v){ FILE *f=fopen("treasure_high.txt","w"); if(f){ fprintf(f,"%d\n",v); fclose(f);} }

int main(){
  srand((unsigned)time(NULL));
  int lvl=1, high = load_high();
#ifndef _WIN32
  enable_raw();
#endif
  printf("\033[2J"); hide_cursor();
  init_level(lvl);
  const int TMS=100; int tick=0;
  while(1){
    draw(lvl);
    int waited=0; char in=0;
    while(waited < TMS){ if(kb_hit()){ in = kb_get(); break; } msleep(10); waited+=10; }
    if(in){ if(in>='A'&&in<='Z') in = in - 'A' + 'a'; if(in=='q') break;
      int nx=px, ny=py; if(in=='w') ny--; else if(in=='s') ny++; else if(in=='a') nx--; else if(in=='d') nx++;
      if(!(nx<1||ny<1||nx>W-2||ny>H-2) && grid[ny][nx] != '#'){
        grid[py][px]=' '; px=nx; py=ny;
        if(grid[py][px]=='$'){ gold_have++; score+=10; }
        if(grid[py][px]=='X'){ show_cursor(); printf("Stepped on TRAP! GAME OVER\n"); goto END; }
        if(grid[py][px]=='M'){ show_cursor(); printf("Caught by ENEMY! GAME OVER\n"); goto END; }
        grid[py][px]='@';
      }
    }
    move_enemies();
    for(int i=0;i<en;i++) if(ex[i]==px && ey[i]==py){ show_cursor(); printf("Enemy hit you! GAME OVER\n"); goto END; }
    tick++; if(tick%3==0) timer_moves--;
    if(timer_moves<=0){ show_cursor(); printf("Time up! GAME OVER\n"); break; }
    if(gold_have >= gold_need){ score+=50; lvl++; for(int r=0;r<H;r++) for(int c=0;c<W;c++) if(grid[r][c]=='M') grid[r][c]=' '; init_level(lvl); }
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
