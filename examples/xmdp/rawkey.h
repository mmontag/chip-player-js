/* librawkey v0.2 - (c) 1994 Russell Marks
 * This library may be freely used/copied/modified provided this copyright
 * notice is left intact.
 */

/* Slightly modified by Claudio Matsuoka for use with xmdp and z81, Apr 97 */

#ifndef __RAWKEY_H
#define __RAWKEY_H

/* Scancodes from Pink Shirt book */

/* Random keys not convered anywhere else below */
#define ESCAPE_KEY	0x01
#define ENTER_KEY	28
#define BACKSPACE	14
#define TAB_KEY		15

/* Shifts */
#define LEFT_SHIFT	0x2A
#define RIGHT_SHIFT	0x36
#define LEFT_CTRL	0x1D
#define LEFT_ALT	0x38

/* NB: right ctrl sends 0xE0 then LEFT_CTRL, right alt sends 0xE0 then
 * LEFT_ALT. If you want to do any shift handling, you probably want to
 * just ignore 0xE0, and look for LEFT_CTRL and LEFT_ALT.
 * note that using scan_keyboard() and is_key_pressed() does this for you.
 */

/* Function keys */

/* this macro lets you do things like FUNC_KEY(1), FUNC_KEY(2), etc. up to
 * FUNC_KEY(12).
 * don't use any side-effects with it.
 */
#define FUNC_KEY(z)	(0x3A+(z)+(((z)>10)?18:0))

/* Cursors, pgup, pgdn, etc. */

#define CURSOR_LEFT	0x4B
#define CURSOR_RIGHT	0x4D
#define CURSOR_UP	0x48
#define CURSOR_DOWN	0x50
#define KEYPAD_CENTER	0x4C	/* the '5' in the centre of the keypad */

#define INSERT_KEY	0x52
#define DELETE_KEY	0x53
#define HOME_KEY	0x47
#define END_KEY		0x4F
#define PAGE_UP		0x49
#define PAGE_DOWN	0x51

/* NB: the 'grey' cursors, pgup, pgdn etc. generate 0xE0 before sending the
 * above codes. The easiest way to deal with this is to ignore 0xE0. :)
 */

#define CAPS_LOCK	0x3A
#define NUM_LOCK	0x45
#define SCROLL_LOCK	0x46

/* PrintScreen generates E0, 2A, E0, 37.	(0x63?)
 * Pause generates E1, 10, 45.			(0x77?)
 * I leave it up to you how to figure those two out properly,
 * but the easiest way is to ignore them. :-/
 */

#define GRAY_PLUS	0x4E
#define GRAY_MINUS	0x4A
#define GRAY_MULTIPLY	0x37	/* NB: also gen'd by PrtSc, see above */
#define GRAY_DIVIDE	0x36	/* NB: prefixed by 0xE0 */



/* For most other keys, you should use the keymap_trans() function to
 * convert the scancode to whatever the keymap would normally generate.
 */



/* Prototypes */

/* NB: it is *vital* that you call rawmode_exit() when you finish, or
 * else you'll be left with the keyboard translation in RAW mode! Not Good.
 * Consider setting up a SIGSEGV handler that calls it, etc. just in case.
 */

int rawmode_init(void);		/* Call this to start */
void rawmode_exit(void);	/* Call this to end */
int is_key_pressed(int);
void scan_keyboard(void);
int get_scancode(void);		/* Returns scancode or -1 if no key pressed */
int keymap_trans(int);		/* Effectively translates scancode to ASCII */
int scancode_trans(int);	/* Effectively xlates ASCII to scancode */

/* Call like: set_switch_functions(undrawfunc,redrawfunc);
 * where undrawfunc() puts the screen in text mode, and
 *       redrawfunc() goes back to graphics and redraws the screen.
 * after the above call, you can do allow_switch(1); to enable
 * VC switching.
 */
void set_switch_functions(void(*)(void),void(*)(void));
void allow_switch(int);		/* Allow VT switch if on=1 */

#endif	/* __RAWKEY_H */
