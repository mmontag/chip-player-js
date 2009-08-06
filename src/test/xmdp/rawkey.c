/* librawkey v0.2 - (c) 1994 Russell Marks
 * This library may be freely used/copied/modified provided this copyright
 * notice is left intact.
 *
 * needs keymap support in kernel - been there since 0.99pl12 I think.
 *
 * Unused variables removed by Claudio Matsuoka, Apr 97.
 */

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <linux/kd.h>		/* RAW mode stuff, etc. */
#include <linux/keyboard.h>	/* mainly for NR_KEYS */
#include <linux/vt.h>		/* for VT stuff - nah, really? :) */
#include "rawkey.h"


static int keymap[NR_KEYS];	/* scancode -> ASCII translation */
static int asciimap[256];	/* ASCII -> scancode translation */
static int tty_fd=-1,restart_con=0,alt_pressed=0;
static struct termios new_termio,old_termio;
static int vtswitch_allowed=0;

static int key_down[128];

/* user-supplied functions to 'undraw' and 'redraw' screen
 * if vt switching is allowed.
 */
void (*usermodeon)(void)=NULL;
void (*usermodeoff)(void)=NULL;


/* it's really easy to translate the scancodes these days, we just
 * use the keytable stuff!
 *
 * returns 0 on error, 1 if ok
 */
int get_keyb_map()
{
struct kbentry keyb_ent;
int f;

keyb_ent.kb_table=0;	/* unshifted */
for(f=0;f<NR_KEYS;f++)
  {
  keyb_ent.kb_index=f;
  
  if(ioctl(tty_fd,KDGKBENT,(unsigned int)&keyb_ent))
    return(0);
    
  keymap[f]=keyb_ent.kb_value;
  }
return(1);
}


void make_ascii_map()
{
int f,i,found;

for(f=0;f<256;f++)
  {
  found=0;
  for(i=0;i<NR_KEYS;i++)
    if((KTYP(keymap[i])==KT_LATIN || KTYP(keymap[i])==KT_LETTER ||
        KTYP(keymap[i])==KT_ASCII) && KVAL(keymap[i])==f)
      {
      found=1;
      break;
      }
      
  asciimap[f]=found?i:-1;
  }
}


void allow_switch(on)
int on;
{
vtswitch_allowed=on;
}


void set_switch_functions(off,on)
void (*off)(void);
void (*on)(void);
{
usermodeoff=off;
usermodeon=on;
}


void raw_mode(tty_fd,on)
int tty_fd,on;
{
ioctl(tty_fd,KDSKBMODE,on?K_RAW:K_XLATE);
}


void blank_key_down()
{
int f;

for(f=0;f<NR_KEYS;f++)
  key_down[f]=0;
}


void vt_from_here()
{
ioctl(tty_fd,TCSETSW,&old_termio);
usermodeoff();
raw_mode(tty_fd,0);  /* don't use rawmode_exit 'cos of other things it does */
ioctl(tty_fd,VT_RELDISP,VT_ACKACQ);
signal(SIGUSR1,vt_from_here);
}


void vt_to_here()
{
usermodeon();
restart_con=1;		/* we're back, say to start up again */
alt_pressed=0;
raw_mode(tty_fd,1);
signal(SIGUSR2,vt_to_here);
}


/* returns 1 if ok, 0 otherwise */
int rawmode_init()
{
if(tty_fd==-1)
  {
  tty_fd=fileno(stdin);
  fcntl(tty_fd,F_SETFL,O_NONBLOCK);
  }

/* fix termio stuff so ^C-style interrupts are ignored */
ioctl(tty_fd,TCGETS,&old_termio);
new_termio=old_termio;
new_termio.c_lflag&=~(ISIG|ICANON);
ioctl(tty_fd,TCSETSW,&new_termio);

if(get_keyb_map(tty_fd,keymap))
  {
  struct vt_mode vtm;
    
  make_ascii_map();
  blank_key_down();
  raw_mode(tty_fd,1);
  signal(SIGUSR1,vt_from_here);
  signal(SIGUSR2,vt_to_here);
  ioctl(tty_fd,VT_GETMODE,&vtm);
  vtm.mode=VT_PROCESS;
  vtm.relsig=SIGUSR1;
  vtm.acqsig=SIGUSR2;
  ioctl(tty_fd,VT_SETMODE,&vtm);
  
  return(1);
  }
else
  return(0);
}


void rawmode_exit()
{
struct vt_mode vtm;

ioctl(tty_fd,VT_GETMODE,&vtm);
vtm.mode=VT_AUTO;
ioctl(tty_fd,VT_SETMODE,&vtm);
ioctl(tty_fd,TCSETSW,&old_termio);
raw_mode(tty_fd,0);
tty_fd=-1;
}


/* returns -1 if no keypresses pending, else returns scancode. */
int get_scancode()
{
unsigned char c;

if(read(tty_fd,&c,1)<=0)
  return(-1);

return((int)c);
}


int is_key_pressed(sc)
int sc;
{
if(sc<0 || sc>127) return(0);
return(key_down[sc]?1:0);
}


int is_any_key_pressed()
{
int f;

for(f=0;f<128;f++)
  if(key_down[f]) return(1);

return(0);
}


/* this is the routine you should call whenever you would normally
 * read a keypress. However, to actually tell if a key is pressed,
 * call is_key_pressed() with a scancode as arg.
 */
void scan_keyboard()
{
int c,key,flag;

/* we use BFI to fix the PrtSc/Pause problem - i.e. we don't :^) */
while((c=get_scancode())==0xE0);
if(c==0xE1) c=get_scancode();

if(c==-1) return;	/* no key was pressed */

key=c&127;
flag=(c&128)?0:1;	/* 1 = down */

key_down[key]=flag;

if(key==LEFT_ALT) alt_pressed=!flag;

if(alt_pressed && c>=FUNC_KEY(1) && c<=FUNC_KEY(10))
  {
  struct vt_stat vts;
  int newvt;
  
  ioctl(tty_fd,VT_GETSTATE,&vts);
  newvt=c-FUNC_KEY(1)+1;
  if(vts.v_active!=newvt && vtswitch_allowed &&
  	usermodeoff!=NULL && usermodeon!=NULL)
    {
    ioctl(tty_fd,VT_ACTIVATE,newvt);
    restart_con=0;
    while(restart_con==0) usleep(50000);
    }
  }
}


/* converts scancode to key binding */
int keymap_trans(sc)
int sc;
{
if(sc<0 || sc>127) return(-1);
return(keymap[sc]);
}


/* converts ASCII to scancode */
int scancode_trans(asc)
int asc;
{
if(asc<0 || asc>255) return(-1);
return(asciimap[asc]);
}
