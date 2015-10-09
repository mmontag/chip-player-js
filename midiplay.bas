0 REM MIDI PLAYER EXAMPLE PROGRAM COPYRIGHT (C) 2010 JOEL YLILUOMA
1 DEFINT A-Z: OPTION BASE 0: KEY OFF:   REM http://iki.fi/bisqwit/

2 DIM adl(180, 11), chins(17), chpan(17), chpit(17)
3 FOR i = 0 TO 180: FOR x = 0 TO 11: READ adl(i, x): NEXT x, i
4 GOTO 100 ' Begin program

5 'Set up OPL parameters (In: c = channel (0..17))
6 'Out: p=I/O port, q = per-OPL channel (0..8), o=operator offset
7 p = &H388 + 2 * (c\9): q = c MOD 9: o = (q MOD 3) + 8 * (q\3): RETURN

8 'OPL_NoteOff(c): c=channel. CHANGES: q,p,o
9 GOSUB 5: OUT p, &HB0 + q: OUT p+1, chpit(c) AND &HDF: RETURN

10 'OPL_NoteOn(c,h#): c=channel,h#=hertz (0..131071). CHANGES: q,p,o,x
11 GOSUB 5
12 x = &H2000: WHILE h# >= 1023.5: h# = h#/2: x=x+&H400: WEND 'Calculate octave
13 x = x + CINT(h#)
14 OUT p, &HA0 + q: OUT p+1, x AND 255: x = x \ 256
15 OUT p, &HB0 + q: OUT p+1, x: chpit(c) = x: RETURN

16 'Technically, AdLib requires some delay between consecutive OUT commands.
17 'However, BASIC is slow enough, that this is not an issue really.
18 'The paradigm "OUT p,index: OUT p+1,value" works perfectly.

20 'OPL_Touch(c,v): c=channel, v=volume. CHANGES: q,p,o,i,v
21 'The formula below: SOLVE(V=127*127 * 2^( (A-63) / 8), A)
22 IF v < 72 THEN v = 0 ELSE v = LOG(v) * 11.541561# - 48.818955#
25 'OPL_Touch_Real(c,v): Same as OPL_Touch, except takes logarithmic volume.
26 GOSUB 5: i = chins(c)
27 OUT p, &H40+o: q = adl(i, 2): GOSUB 29
28 OUT p, &H43+o: q = adl(i, 3)
29 OUT p+1, (q OR 63) - v + ((q AND 63) * v) \ 63: RETURN

30 'OPL_Patch(c): c=channel. CHANGES: q,p,o,x,i
31 GOSUB 5: i = chins(c): q = p+1
32 FOR x = 0 TO 1
33  OUT p, &H20+o+x*3: OUT q, adl(i, 0+x)
34  OUT p, &H60+o+x*3: OUT q, adl(i, 4+x)
35  OUT p, &H80+o+x*3: OUT q, adl(i, 6+x)
36  OUT p, &HE0+o+x*3: OUT q, adl(i, 8+x)
37 NEXT: RETURN

38 'OPL_Pan(c): c=channel. CHANGES: q,p,o
39 GOSUB 5: OUT p, &HC0+q: OUT p+1, adl(chins(c),10)-chpan(c): RETURN

40 'OPL_Reset(). CHANGES: c,q,p,o,x,y,v,i
41 'Detect OPL3 (send pulses to timer, opl3 enable register)
42 c=0: GOSUB 5: FOR y = 3 TO 4: OUT p, 4: OUT p+1, y*32: NEXT: ?INP(p)
43 c=9: GOSUB 5: FOR y = 0 TO 2: OUT p, 5: OUT p+1, y AND 1: NEXT
44 'Reset OPL3 (turn off all notes, set vital settings)
45 c=0: GOSUB 5: OUT p, 1:  OUT p+1, 32 'Enable wave
46 c=0: GOSUB 5: OUT p,&HBD:OUT p+1, 0  'Set melodic mode
47 c=9: GOSUB 5: OUT p, 5:  OUT p+1, 1  'Enable OPL3
48 c=9: GOSUB 5: OUT p, 4:  OUT p+1, 0  'Select mode 0 (also try 63 for fun!)
50 'OPL_Silence(): Silence all OPL channels. CHANGES q,p,o,v,c,i
51 v = 0: FOR c = 0 TO 17: GOSUB 8: GOSUB 25: NEXT: RETURN

70 'ReadString(): Read N bytes from file. Input: x. Output: s$. CHANGES x
71 s$ = "": WHILE x > 0: x = x - 1: GET #1: s$ = s$ + b$: WEND: RETURN

75 'ReadVarLen(): Read variable length int from file. Output: x#. CHANGES x
76 x# = 0
77 GET #1: x = ASC(b$): x# = x# * 128 + (x AND 127): IF x >= 128 THEN 77
78 RETURN

80 'ConvertInteger(): Parses s$ as big-endian integer, stores to x#. CHANGES x
81 x# = 0: FOR x=1 TO LEN(s$): x# = x# * 256 + ASC(MID$(s$, x, 1)): NEXT: RETURN
82 'The reason why # (DOUBLE) is used to store integer values
83 'is that GW-BASIC has only one integer type: INTEGER.
84 'It supports values in the range -32768..+32737. Making it unsuitable
85 'for storing values such as delta-times and track lengths, both of which
86 'are often greater than +32767 in common MIDI files. QuickBASIC supports
87 'the LONG datatype instead, but I was aiming for GW-BASIC compatibility
88 'in order to utilize my cool syntax highlighter for the majority
89 'of the duration of demonstrating this program on Youtube. :)

94 'Ethical subroutine
95 GOTO 97
96 KILL filename$
97 RETURN

100 '*** MAIN MIDI PLAYER PROGRAM ***
101 'Information about each track:
102 DIM tkPtr%(100), tkDelay#(100), tkStatus(100), playwait#
103 'The same, but for loop-beginning:
104 DIM loPtr%(100), loDelay#(100), loStatus(100), loopwait#
105 'The same, but cached just in case loop-beginning must be written:
106 DIM rbPtr%(100), rbDelay#(100), rbStatus(100)

109 'Persistent settings for each MIDI channel.
110 DIM ChPatch(15), ChBend#(15), ChVolume(15), ChPanning(15), ChVibrato(15)

120 'For each active note, we need the following:
121 '    Original note number (ORIGINAL, not simulated)
122 '       Keyoff      (Search Key)
123 '       Aftertouch  (Search Key)
124 '    Simulated note number
125 '       Keyon -> OPL_NoteOn
126 '       Bend  -> OPL_NoteOn
127 '    Keyon/touch pressure
128 '       Ctrl:Volume
129 '    Adlib channel number
130 '       All of above
131 '    MIDI channel (multi search key)
132 '       Bend
133 '       Ctrl:Volume
134 '       Ctrl:Pan
140 DIM ActCount(15)      'number of active notes on each midi channel
141 DIM ActTone(15,127)   'orignotenumber -> simulated notenumber
142 DIM ActAdlChn(15,127) 'orignotenumber -> adlib channel
143 DIM ActVol(15,127)    'orignotenumber -> pressure
144 DIM ActRev(15,127)    'orignotenumber -> activeindex (index to ActList)
145 DIM ActList(15,99)    'activeindex -> orignotenumber (index to ActVol etc)

146 DIM chon(17),chage#(17)'adlchannel -> is_on,age (for channel allocation)
148 DIM chm(17),cha(17)    'adlchannel -> midichn,activeindex (for collisions)
149 DIM chx(17),chc(17)    'adlchannel -> x coordinate, color (for graphics)

160 filename$ = "chmmr.mid" '<- FILENAME OF THE MIDI TO BE PLAYED
161 GOSUB 40  ' Reset AdLib
162 GOSUB 40  ' ...twice (just in case someone misprogrammed OPL3 previously)
163 PLAYmode$  = "T255P64"
164 PLAYmode%  = 255 * 64 'Event tick frequency
165 PLAYbuflen = 2        'Number of events to keep buffered
166 COLOR 8: CLS : txtline = 2: PRINT "Press Q to quit; Space to pause"
169 GOSUB 200 ' Load and play MIDI file

170 'Begin main program (this simply monitors the playing status)
171 'Beware that the timer routine accesses a lot of global variables.
172 paused = 0
173 hlt(0) = &HCBF4' HLT; RETF.
174   hlt = VARPTR(hlt(0))
175   CALL hlt
176   ' ^This subroutine saves power. It also makes DOSBox faster.
177   ' However, QuickBASIC has a different syntax for calling ASM
178   ' subroutines, so the line 175 will have to be changed when
179   ' porting to QuickBASIC. Disable it, or try this (qb /Lqb.qlb):
180   'DEF SEG = VARSEG(hlt(0)): CALL ABSOLUTE(hlt)

182   y$ = INKEY$
183   IF y$ <> " " THEN 186
184   paused = 1 - paused
185   IF paused THEN ? "Pause": PLAY STOP ELSE ? "Ok": PLAY ON
186 IF y$ <> "q" AND y$ <> CHR$(27) AND y$ <> CHR$(3) THEN 174
190 PLAY OFF: PRINT "End!": GOSUB 50: KEY ON: END

200 'Subroutine: Load MIDI file
210 OPEN filename$ FOR RANDOM AS #1 LEN = 1: FIELD #1, 1 AS b$  'Open file

211 'QuickBASIC has BINARY access mode, which allows to use INPUT$() rather
212 'than the silly fixed-size GET command, but GW-BASIC does not support
213 'BINARY, so we use RANDOM here. The difference between BINARY and INPUT
214 'is that INPUT chokes on EOF characters and translates CRLFs, making it
215 'unsuitable for working with MIDI files.

220 x = 4: GOSUB 70: IF s$ <> "MThd" THEN ERROR 13'Invalid type file
230 x = 4: GOSUB 70: GOSUB 80: IF x# <> 6 THEN ERROR 13
231 x = 2: GOSUB 70: GOSUB 80: Fmt        = x#
232 x = 2: GOSUB 70: GOSUB 80: TrackCount = x#: IF TrackCount>100 THEN ERROR 6
233 x = 2: GOSUB 70: GOSUB 80: DeltaTicks = x#
234 InvDeltaTicks# = PLAYmode% / (240000000# * DeltaTicks)
240 Tempo# = 1000000# * InvDeltaTicks#
241 bendsense# = 2 / 8192#
250 FOR tk = 1 TO TrackCount
251   x = 4: GOSUB 70: IF s$ <> "MTrk" THEN ERROR 13'Invalid type file
252   x = 4: GOSUB 70: GOSUB 80: TrackLength% = x#: y% = LOC(1)
253   GOSUB 75: tkDelay#(tk) = x# 'Read next event time
254   tkPtr%(tk) = LOC(1)         'Save first event file position
255   GET #1, y% + TrackLength%   'Skip to beginning of next track header
270 NEXT
275 FOR a = 0 TO 15: ChVolume(a) = 127: NEXT
281 PLAY ON: ON PLAY(PLAYbuflen) GOSUB 300 'Set up periodic event handler
282 began = 0: loopStart = 1: playwait# = 0: PLAY "MLMB"

300 'Subroutine: Timer callback. Called when the PLAY buffer is exhausted.
301 IF began THEN playwait# = playwait# - 1#
302 'For each track where delay=0, parse events and read new delay.
303 WHILE playwait# < .5#: GOSUB 310: WEND
304 'Repopulate the PLAY buffer
305 WHILE PLAY(0) < PLAYbuflen: PLAY PLAYmode$: WEND: RETURN

310 'Subroutine: Process events on any track that is due
311 FOR tk = 1 TO TrackCount
312   rbPtr%(tk)   = tkPtr%(tk)
313   rbDelay#(tk) = tkDelay#(tk)
314   rbStatus(tk) = tkStatus(tk)
315   IF tkStatus(tk) < 0 OR tkDelay#(tk) > 0 THEN 319
316   GOSUB 350                                  'Handle event
317   GOSUB 75: tkDelay#(tk) = tkDelay#(tk) + x# 'Read next event time
318   tkPtr%(tk) = LOC(1)                        'Save modified file position
319 NEXT
320 IF loopStart = 0 THEN 338
321   'Save loop begin
322   FOR tk = 1 TO TrackCount
323     loPtr%(tk)   = rbPtr%(tk)
324     loDelay#(tk) = rbDelay#(tk)
325     loStatus(tk) = rbStatus(tk)
326   NEXT
327   loopwait# = playwait#
328   loopStart = 0
329   GOTO 338
330   'Return to loop beginning
331   FOR tk = 1 TO TrackCount
332     tkPtr%(tk)   = loPtr%(tk)
333     tkDelay#(tk) = loDelay#(tk)
334     tkStatus(tk) = loStatus(tk)
335   NEXT
336   loopEnd   = 0
337   playwait# = loopwait#
338 'Find shortest delay from all tracks
339 nd# = -1
340 FOR tk = 1 TO TrackCount
341   IF tkStatus(tk) < 0 THEN 343
342   IF nd# = -1 OR tkDelay#(tk) < nd# THEN nd# = tkDelay#(tk)
343 NEXT
344 'Schedule the next playevent to be processed after that delay
345 FOR tk = 1 TO TrackCount: tkDelay#(tk) = tkDelay#(tk) - nd#: NEXT
346 t# = nd# * Tempo#: IF began THEN playwait# = playwait# + t#
347 FOR a = 0 TO 17: chage#(a) = chage#(a) + t#: NEXT
348 IF t# < 0 OR loopEnd THEN 330 ELSE RETURN 'Loop or done

350 'Subroutine: Read an event from track, and read next delay. Input: tk

351 'Note that we continuously access the disk file during playback.
352 'This is perfectly fine when we are running on a modern OS, possibly
353 'under the encapsulation of DOSBox, but if you're running this on
354 'vanilla MS-DOS without e.g. SMARTDRV, you are going to find this
355 'program very unsuitable for your MIDI playing needs, for the disk
356 'drive would probably make more noise than the soundcard does.
357 'We cannot cache tracks because of memory constraints in GW-BASIC.
358 'In QBASIC, we could do better, but it would be cumbersome to implement.

359 GET #1, tkPtr%(tk) + 1: b = ASC(b$)
360 IF b < &HF0 THEN 380
361 'Subroutine for Fx special events
362 '? tk;":";HEX$(b);" at $";hex$(LOC(1))
363 IF b = &HF7 OR b = &HF0 THEN 76'SysEx. Ignore Varlen.
364 IF b = &HF3 THEN GET #1: RETURN
365 IF b = &HF2 THEN GET #1: GET #1: RETURN
370 'Subroutine for special event FF
371 GET #1: evtype = ASC(b$): GOSUB 75: x = x#: GOSUB 70
372 IF evtype = &H2F THEN tkStatus(tk) = -1
373 IF evtype = &H51 THEN GOSUB 80: Tempo# = x# * InvDeltaTicks#
374 IF evtype = 6 AND s$ = "loopStart" THEN loopStart = 1
375 IF evtype = 6 AND s$ = "loopEnd"   THEN loopEnd   = 1
376 IF evtype < 1 OR evtype > 6 THEN RETURN
377 txtline = 3 + (txtline - 2) MOD 20
378 LOCATE txtline, 1: COLOR 8: PRINT "Meta"; evtype; ": "; s$: RETURN

380 'Subroutine for any normal event (80..EF)
381 IF b < 128 THEN b = tkStatus(tk) OR &H80: GET #1, tkPtr%(tk)
382 MidCh = b AND 15: tkStatus(tk) = b
383 '? tk;":";HEX$(b);" at $";hex$(LOC(1))
384 ON b\16 - 7 GOTO 400,420,460,470,490,492,495  'Choose event handler

400 'Event: 8x Note Off
401 GET #1: note = ASC(b$): GET #1': vol=ASC(b$)
402 ChBend#(MidCh) = 0
403 n = ActRev(MidCh,note): IF n = 0 THEN RETURN
404 m = MidCh: GOTO 600 'deallocate active note, and return

420 'Event: 9x Note On
421 GET #1: note = ASC(b$): GET #1: vol = ASC(b$)
422 IF vol = 0 THEN 402' Sometimes noteoffs are optimized as 0-vol noteons
423 IF ActRev(MidCh,note) THEN RETURN 'Ignore repeat notes w/o keyoffs
424 'Determine the instrument and the note value (tone)
425 tone = note: i = ChPatch(MidCh)
426 IF MidCh = 9 THEN i = 128+note-35: tone = adl(i,11) 'Translate percussion
427 '(MIDI channel 9 always plays percussion and ignores the patch number)
429 'Allocate AdLib channel (the physical sound channel for the note)
430 bs# = -9: c = -1
431 FOR a = 0 TO 17
432  s# = chage#(a)
433  IF chon(a)  = 0 THEN s# = s# + 3d3  ' Empty channel = privileged
434  IF chins(a) = i THEN s# = s# + .2#  ' Same instrument = good
435  IF i<128 AND chins(a)>127 THEN s# = s#*2+9'Percussion is inferior to melody
436  IF s# > bs# THEN bs# = s#: c = a  ' Best candidate wins
437 NEXT
438 IF chon(c) THEN m=chm(c): n=cha(c): GOSUB 600  'Collision: Kill old note
439 chon(c) = 1: chins(c) = i: chage#(c) = 0: began = 1
440 'Allocate active note for MIDI channel
441 '"Active note" helps dealing with further events affecting this note.
442 n = ActCount(MidCh) + 1
443 ActList(MidCh, n)     = note
444 ActRev(MidCh,note)    = n
445 ActCount(MidCh)       = n
449 'Record info about this note
450 ActTone(MidCh,note)   = tone
451 ActAdlChn(MidCh,note) = c
452 ActVol(MidCh,note)    = vol
453 chm(c) = MidCh: cha(c) = n ' Save this note's origin so collision works.
454 GOSUB 30 ' OPL_Patch
455 GOSUB 530' OPL_Pan with ChPanning
456 GOSUB 540' OPL_Touch with ChVolume
457 chx(c) = 1 + (tone+63)MOD 80: chc(c) = 9+(chins(c)MOD 6)
458 LOCATE 20-c, chx(c): COLOR chc(c): PRINT "#";
459 GOTO 520 ' OPL_NoteOn with ChBend, and return

460 'Event: Ax Note touch
461 GET #1: note = ASC(b$): GET #1: vol = ASC(b$)
462 IF ActRev(MidCh,note) = 0 THEN RETURN'Ignore touch if note is not active
463 c = ActAdlChn(MidCh,note)
464 LOCATE 20-c, chx(c): COLOR chc(c): PRINT "&";
465 ActVol(MidCh,note) = vol
466 GOTO 540 'update note volume, and return

470 'Event: Bx Controller change
471 GET #1: ctrlno = ASC(b$): GET #1: value = ASC(b$)
472 IF ctrlno =   1 THEN ChVibrato(MidCh) = value 'TODO: handle
473 IF ctrlno =   6 THEN bendsense# = value / 8192#
474 IF ctrlno =   7 THEN ChVolume(MidCh) = value: mop = 2: GOTO 500
475 IF ctrlno =  10 THEN 482 'Pan
476 IF ctrlno = 121 THEN 486 'Reset controllers
477 IF ctrlno = 123 THEN mop=5: GOTO 500 'All notes off on channel
478 'Other ctrls worth considering:
479 '  0 = choose bank (bank+patch identifies the instrument)
480 ' 64 = sustain pedal (when used, noteoff does not produce an adlib keyoff)
481 RETURN
482 'Ctrl 10: Alter the panning of the channel:
483 p = 0: IF value < 48 THEN p = 32 ELSE IF value > 79 THEN p = 16
484 ChPanning(MidCh) = p
485 mop=4: GOTO 500
486 'Ctrl 121: Reset all controllers to their default values.
487 ChBend#(MidCh)=0: ChVibrato(MidCh)=0: ChPan(MidCh)=0
488 mop=1: GOSUB 500: mop=2: GOSUB 500: mop=4: GOTO 500

490 'Event: Cx Patch change
491 GET #1: ChPatch(MidCh) = ASC(b$): RETURN

492 'Event: Dx Channel after-touch
493 GET #1: vol = ASC(b$): mop=3:GOTO 500 'TODO: Verify, is this correct action?

495 'Event: Ex Wheel/bend
496 GET #1: a = ASC(b$): GET #1
497 ChBend#(MidCh) = (a + ASC(b$) * 128 - 8192) * bendsense#
498 mop = 1: GOTO 500 'Update pitches, and return

500 'Subroutine: Update all live notes. Input: MidCh,mop
501 'Update when mop: 1=pitches; 2=volumes; 3=pressures; 4=pans, 5=off
502 x1 = ActCount(MidCh)
503 FOR a = 1 TO x1
504   note = ActList(MidCh, a)
505   c    = ActAdlChn(MidCh,note): ON mop GOSUB 508,509,510,530,511
506 NEXT
507 RETURN
508 tone = ActTone(MidCh,note): GOTO 520
509 vol  = ActVol(MidCh, note): GOTO 540
510 ActVol(MidCh,note) = vol: GOTO 540
511 m = MidCh: n = a: GOTO 600

520 'Subroutine: Update note pitch. Input: c,MidCh,tone. CHANGES q,p,o,x,h#
521 ' 907*2^(n/12) * (8363) / 44100
522 h# = 172.00093# * EXP(.057762265#*(tone+ChBend#(MidCh))): GOTO 10'OPL_NoteOn

530 'Subroutine: Update note pan. Input: c,MidCh
531 chpan(c) = ChPanning(MidCh): GOTO 38 'OPL_Pan

540 'Subroutine: Update note volume. Input: c,MidCh,vol. CHANGES q,p,o,v
541 v = vol * ChVolume(MidCh): GOTO 20 'OPL_Touch

600 'Subroutine: Deallocate active note (m = MidCh, n = index). CHANGES c,x,q
601 'Uses m instead of MidCh because called also from alloc-collision code.
602 x = ActCount(m)    ' Find how many active notes
603 q = ActList(m,n)   ' q=note to be deactivated
604 ActRev(m,q) = 0    ' that note is no more
605 ActCount(m) = x-1  ' The list is now shorter
606 c = ActAdlChn(m,q) ' But wait, which adlib channel was it on, again?
607 chon(c)=0: chage#(c)=0: GOSUB 8'OPL_NoteOff
608 LOCATE 20-c, chx(c): COLOR 1: PRINT ".";
610 IF n = x THEN RETURN' Did we delete the last note?
611 q = ActList(m,x)    ' q = last note in list
612 ActList(m,n) = q    ' move into the deleted slot
613 ActRev(m,q)  = n
614 cha(ActAdlChn(m,q)) = n
615 RETURN

760 'This FM Instrument Data comes from Miles Sound System, as used
761 'in the following PC games: Star Control III and Asterix, under
762 'the name AIL (Audio Interface Library). Today, AIL has been
763 'released as "open-source freeware" under the name Miles Sound System.
764 'AIL was used in more than fifty PC games, but so far, I have found
765 'this particular set of General MIDI FM patches only in SC3 and Asterix.
766 'Other games using AIL used different sets of FM patches. There is no
767 'particular reason for preferring this patch set, and in fact, the
768 'descendant of this program, ADLMIDI, http://iki.fi/source/adlmidi.html ,
769 'features a large set of different FM patch sets to choose from.

773 'In the Youtube video, I enter this huge blob of DATA lines very quickly
774 'by using a preprogrammed input TSR, "inputter", which I made just for
775 'this purpose.
776 'It inputs the inline command "COLOR 0,4", turning text into black on red,
777 'and then starts entering DATA lines in an arbitrary geometrical fashion.
778 'After inputting, the text color is reset to normal with "COLOR 7,0".
779 'Of course, the whole time, the poor syntax highlighter TSR (synhili)
780 'does its best to make sense of whatever is being displayed on the screen,
781 'causing the text to stay red only for a short while, whereever the cursor
782 'was last. The resulting effect looks very cool, and most importantly,
783 'the long sequence of DATA gets input in a non-boring manner.

790 'The data bytes are:
791 ' [0,1] AM/VIB/EG/KSR/Multiple bits for carrier and modulator respectively
792 ' [2,3] KSL/Attenuation settings    for carrier and modulator respectively
793 ' [4,5] Attack and decay rates      for carrier and modulator respectively
794 ' [6,7] Sustain and release rates   for carrier and modulator respectively
795 ' [8,9] Wave select settings        for carrier and modulator respectively
796 ' [10]  Feedback/connection bits    for the channel (also stereo/pan bits)
797 ' [11]  For percussive instruments (GP35..GP87), the tone to play

800 DATA   1,  1,143,  6,242,242,244,247,0,0, 56,  0: REM GM1:AcouGrandPiano
801 DATA   1,  1, 75,  0,242,242,244,247,0,0, 56,  0: REM GM2:BrightAcouGrand
802 DATA   1,  1, 73,  0,242,242,244,246,0,0, 56,  0: REM GM3:ElecGrandPiano
803 DATA 129, 65, 18,  0,242,242,247,247,0,0, 54,  0: REM GM4:Honky-tonkPiano
804 DATA   1,  1, 87,  0,241,242,247,247,0,0, 48,  0: REM GM5:Rhodes Piano
805 DATA   1,  1,147,  0,241,242,247,247,0,0, 48,  0: REM GM6:Chorused Piano
806 DATA   1, 22,128, 14,161,242,242,245,0,0, 56,  0: REM GM7:Harpsichord
807 DATA   1,  1,146,  0,194,194,248,248,0,0, 58,  0: REM GM8:Clavinet
808 DATA  12,129, 92,  0,246,243,244,245,0,0, 48,  0: REM GM9:Celesta
809 DATA   7, 17,151,128,243,242,242,241,0,0, 50,  0: REM GM10:Glockenspiel
810 DATA  23,  1, 33,  0, 84,244,244,244,0,0, 50,  0: REM GM11:Music box
811 DATA 152,129, 98,  0,243,242,246,246,0,0, 48,  0: REM GM12:Vibraphone
812 DATA  24,  1, 35,  0,246,231,246,247,0,0, 48,  0: REM GM13:Marimba
813 DATA  21,  1,145,  0,246,246,246,246,0,0, 52,  0: REM GM14:Xylophone
814 DATA  69,129, 89,128,211,163,243,243,0,0, 60,  0: REM GM15:Tubular Bells
815 DATA   3,129, 73,128,117,181,245,245,1,0, 52,  0: REM GM16:Dulcimer
816 DATA 113, 49,146,  0,246,241, 20,  7,0,0, 50,  0: REM GM17:Hammond Organ
817 DATA 114, 48, 20,  0,199,199, 88,  8,0,0, 50,  0: REM GM18:Percussive Organ
818 DATA 112,177, 68,  0,170,138, 24,  8,0,0, 52,  0: REM GM19:Rock Organ
819 DATA  35,177,147,  0,151, 85, 35, 20,1,0, 52,  0: REM GM20:Church Organ
820 DATA  97,177, 19,128,151, 85,  4,  4,1,0, 48,  0: REM GM21:Reed Organ
821 DATA  36,177, 72,  0,152, 70, 42, 26,1,0, 60,  0: REM GM22:Accordion
822 DATA  97, 33, 19,  0,145, 97,  6,  7,1,0, 58,  0: REM GM23:Harmonica
823 DATA  33,161, 19,137,113, 97,  6,  7,0,0, 54,  0: REM GM24:Tango Accordion
824 DATA   2, 65,156,128,243,243,148,200,1,0, 60,  0: REM GM25:Acoustic Guitar1
825 DATA   3, 17, 84,  0,243,241,154,231,1,0, 60,  0: REM GM26:Acoustic Guitar2
826 DATA  35, 33, 95,  0,241,242, 58,248,0,0, 48,  0: REM GM27:Electric Guitar1
827 DATA   3, 33,135,128,246,243, 34,248,1,0, 54,  0: REM GM28:Electric Guitar2
828 DATA   3, 33, 71,  0,249,246, 84, 58,0,0, 48,  0: REM GM29:Electric Guitar3
829 DATA  35, 33, 74,  5,145,132, 65, 25,1,0, 56,  0: REM GM30:Overdrive Guitar
830 DATA  35, 33, 74,  0,149,148, 25, 25,1,0, 56,  0: REM GM31:Distorton Guitar
831 DATA   9,132,161,128, 32,209, 79,248,0,0, 56,  0: REM GM32:Guitar Harmonics
832 DATA  33,162, 30,  0,148,195,  6,166,0,0, 50,  0: REM GM33:Acoustic Bass
833 DATA  49, 49, 18,  0,241,241, 40, 24,0,0, 58,  0: REM GM34:Electric Bass 1
834 DATA  49, 49,141,  0,241,241,232,120,0,0, 58,  0: REM GM35:Electric Bass 2
835 DATA  49, 50, 91,  0, 81,113, 40, 72,0,0, 60,  0: REM GM36:Fretless Bass
836 DATA   1, 33,139, 64,161,242,154,223,0,0, 56,  0: REM GM37:Slap Bass 1
837 DATA  33, 33,139,  8,162,161, 22,223,0,0, 56,  0: REM GM38:Slap Bass 2
838 DATA  49, 49,139,  0,244,241,232,120,0,0, 58,  0: REM GM39:Synth Bass 1
839 DATA  49, 49, 18,  0,241,241, 40, 24,0,0, 58,  0: REM GM40:Synth Bass 2
840 DATA  49, 33, 21,  0,221, 86, 19, 38,1,0, 56,  0: REM GM41:Violin
841 DATA  49, 33, 22,  0,221,102, 19,  6,1,0, 56,  0: REM GM42:Viola
842 DATA 113, 49, 73,  0,209, 97, 28, 12,1,0, 56,  0: REM GM43:Cello
843 DATA  33, 35, 77,128,113,114, 18,  6,1,0, 50,  0: REM GM44:Contrabass
844 DATA 241,225, 64,  0,241,111, 33, 22,1,0, 50,  0: REM GM45:Tremulo Strings
845 DATA   2,  1, 26,128,245,133,117, 53,1,0, 48,  0: REM GM46:Pizzicato String
846 DATA   2,  1, 29,128,245,243,117,244,1,0, 48,  0: REM GM47:Orchestral Harp
847 DATA  16, 17, 65,  0,245,242,  5,195,1,0, 50,  0: REM GM48:Timpany
848 DATA  33,162,155,  1,177,114, 37,  8,1,0, 62,  0: REM GM49:String Ensemble1
849 DATA 161, 33,152,  0,127, 63,  3,  7,1,1, 48,  0: REM GM50:String Ensemble2
850 DATA 161, 97,147,  0,193, 79, 18,  5,0,0, 58,  0: REM GM51:Synth Strings 1
851 DATA  33, 97, 24,  0,193, 79, 34,  5,0,0, 60,  0: REM GM52:SynthStrings 2
852 DATA  49,114, 91,131,244,138, 21,  5,0,0, 48,  0: REM GM53:Choir Aahs
853 DATA 161, 97,144,  0,116,113, 57,103,0,0, 48,  0: REM GM54:Voice Oohs
854 DATA 113,114, 87,  0, 84,122,  5,  5,0,0, 60,  0: REM GM55:Synth Voice
855 DATA 144, 65,  0,  0, 84,165, 99, 69,0,0, 56,  0: REM GM56:Orchestra Hit
856 DATA  33, 33,146,  1,133,143, 23,  9,0,0, 60,  0: REM GM57:Trumpet
857 DATA  33, 33,148,  5,117,143, 23,  9,0,0, 60,  0: REM GM58:Trombone
858 DATA  33, 97,148,  0,118,130, 21, 55,0,0, 60,  0: REM GM59:Tuba
859 DATA  49, 33, 67,  0,158, 98, 23, 44,1,1, 50,  0: REM GM60:Muted Trumpet
860 DATA  33, 33,155,  0, 97,127,106, 10,0,0, 50,  0: REM GM61:French Horn
861 DATA  97, 34,138,  6,117,116, 31, 15,0,0, 56,  0: REM GM62:Brass Section
862 DATA 161, 33,134,131,114,113, 85, 24,1,0, 48,  0: REM GM63:Synth Brass 1
863 DATA  33, 33, 77,  0, 84,166, 60, 28,0,0, 56,  0: REM GM64:Synth Brass 2
864 DATA  49, 97,143,  0,147,114,  2, 11,1,0, 56,  0: REM GM65:Soprano Sax
865 DATA  49, 97,142,  0,147,114,  3,  9,1,0, 56,  0: REM GM66:Alto Sax
866 DATA  49, 97,145,  0,147,130,  3,  9,1,0, 58,  0: REM GM67:Tenor Sax
867 DATA  49, 97,142,  0,147,114, 15, 15,1,0, 58,  0: REM GM68:Baritone Sax
868 DATA  33, 33, 75,  0,170,143, 22, 10,1,0, 56,  0: REM GM69:Oboe
869 DATA  49, 33,144,  0,126,139, 23, 12,1,1, 54,  0: REM GM70:English Horn
870 DATA  49, 50,129,  0,117, 97, 25, 25,1,0, 48,  0: REM GM71:Bassoon
871 DATA  50, 33,144,  0,155,114, 33, 23,0,0, 52,  0: REM GM72:Clarinet
872 DATA 225,225, 31,  0,133,101, 95, 26,0,0, 48,  0: REM GM73:Piccolo
873 DATA 225,225, 70,  0,136,101, 95, 26,0,0, 48,  0: REM GM74:Flute
874 DATA 161, 33,156,  0,117,117, 31, 10,0,0, 50,  0: REM GM75:Recorder
875 DATA  49, 33,139,  0,132,101, 88, 26,0,0, 48,  0: REM GM76:Pan Flute
876 DATA 225,161, 76,  0,102,101, 86, 38,0,0, 48,  0: REM GM77:Bottle Blow
877 DATA  98,161,203,  0,118, 85, 70, 54,0,0, 48,  0: REM GM78:Shakuhachi
878 DATA  98,161,153,  0, 87, 86,  7,  7,0,0, 59,  0: REM GM79:Whistle
879 DATA  98,161,147,  0,119,118,  7,  7,0,0, 59,  0: REM GM80:Ocarina
880 DATA  34, 33, 89,  0,255,255,  3, 15,2,0, 48,  0: REM GM81:Lead 1 squareea
881 DATA  33, 33, 14,  0,255,255, 15, 15,1,1, 48,  0: REM GM82:Lead 2 sawtooth
882 DATA  34, 33, 70,128,134,100, 85, 24,0,0, 48,  0: REM GM83:Lead 3 calliope
883 DATA  33,161, 69,  0,102,150, 18, 10,0,0, 48,  0: REM GM84:Lead 4 chiff
884 DATA  33, 34,139,  0,146,145, 42, 42,1,0, 48,  0: REM GM85:Lead 5 charang
885 DATA 162, 97,158, 64,223,111,  5,  7,0,0, 50,  0: REM GM86:Lead 6 voice
886 DATA  32, 96, 26,  0,239,143,  1,  6,0,2, 48,  0: REM GM87:Lead 7 fifths
887 DATA  33, 33,143,128,241,244, 41,  9,0,0, 58,  0: REM GM88:Lead 8 brass
888 DATA 119,161,165,  0, 83,160,148,  5,0,0, 50,  0: REM GM89:Pad 1 new age
889 DATA  97,177, 31,128,168, 37, 17,  3,0,0, 58,  0: REM GM90:Pad 2 warm
890 DATA  97, 97, 23,  0,145, 85, 52, 22,0,0, 60,  0: REM GM91:Pad 3 polysynth
891 DATA 113,114, 93,  0, 84,106,  1,  3,0,0, 48,  0: REM GM92:Pad 4 choir
892 DATA  33,162,151,  0, 33, 66, 67, 53,0,0, 56,  0: REM GM93:Pad 5 bowedpad
893 DATA 161, 33, 28,  0,161, 49,119, 71,1,1, 48,  0: REM GM94:Pad 6 metallic
894 DATA  33, 97,137,  3, 17, 66, 51, 37,0,0, 58,  0: REM GM95:Pad 7 halo
895 DATA 161, 33, 21,  0, 17,207, 71,  7,1,0, 48,  0: REM GM96:Pad 8 sweep
896 DATA  58, 81,206,  0,248,134,246,  2,0,0, 50,  0: REM GM97:FX 1 rain
897 DATA  33, 33, 21,  0, 33, 65, 35, 19,1,0, 48,  0: REM GM98:FX 2 soundtrack
898 DATA   6,  1, 91,  0,116,165,149,114,0,0, 48,  0: REM GM99:FX 3 crystal
899 DATA  34, 97,146,131,177,242,129, 38,0,0, 60,  0: REM GM100:FX 4 atmosphere
900 DATA  65, 66, 77,  0,241,242, 81,245,1,0, 48,  0: REM GM101:FX 5 brightness
901 DATA  97,163,148,128, 17, 17, 81, 19,1,0, 54,  0: REM GM102:FX 6 goblins
902 DATA  97,161,140,128, 17, 29, 49,  3,0,0, 54,  0: REM GM103:FX 7 echoes
903 DATA 164, 97, 76,  0,243,129,115, 35,1,0, 52,  0: REM GM104:FX 8 sci-fi
904 DATA   2,  7,133,  3,210,242, 83,246,0,1, 48,  0: REM GM105:Sitar
905 DATA  17, 19, 12,128,163,162, 17,229,1,0, 48,  0: REM GM106:Banjo
906 DATA  17, 17,  6,  0,246,242, 65,230,1,2, 52,  0: REM GM107:Shamisen
907 DATA 147,145,145,  0,212,235, 50, 17,0,1, 56,  0: REM GM108:Koto
908 DATA   4,  1, 79,  0,250,194, 86,  5,0,0, 60,  0: REM GM109:Kalimba
909 DATA  33, 34, 73,  0,124,111, 32, 12,0,1, 54,  0: REM GM110:Bagpipe
910 DATA  49, 33,133,  0,221, 86, 51, 22,1,0, 58,  0: REM GM111:Fiddle
911 DATA  32, 33,  4,129,218,143,  5, 11,2,0, 54,  0: REM GM112:Shanai
912 DATA   5,  3,106,128,241,195,229,229,0,0, 54,  0: REM GM113:Tinkle Bell
913 DATA   7,  2, 21,  0,236,248, 38, 22,0,0, 58,  0: REM GM114:Agogo Bells
914 DATA   5,  1,157,  0,103,223, 53,  5,0,0, 56,  0: REM GM115:Steel Drums
915 DATA  24, 18,150,  0,250,248, 40,229,0,0, 58,  0: REM GM116:Woodblock
916 DATA  16,  0,134,  3,168,250,  7,  3,0,0, 54,  0: REM GM117:Taiko Drum
917 DATA  17, 16, 65,  3,248,243, 71,  3,2,0, 52,  0: REM GM118:Melodic Tom
918 DATA   1, 16,142,  0,241,243,  6,  2,2,0, 62,  0: REM GM119:Synth Drum
919 DATA  14,192,  0,  0, 31, 31,  0,255,0,3, 62,  0: REM GM120:Reverse Cymbal
920 DATA   6,  3,128,136,248, 86, 36,132,0,2, 62,  0: REM GM121:Guitar FretNoise
921 DATA  14,208,  0,  5,248, 52,  0,  4,0,3, 62,  0: REM GM122:Breath Noise
922 DATA  14,192,  0,  0,246, 31,  0,  2,0,3, 62,  0: REM GM123:Seashore
923 DATA 213,218,149, 64, 55, 86,163, 55,0,0, 48,  0: REM GM124:Bird Tweet
924 DATA  53, 20, 92,  8,178,244, 97, 21,2,0, 58,  0: REM GM125:Telephone
925 DATA  14,208,  0,  0,246, 79,  0,245,0,3, 62,  0: REM GM126:Helicopter
926 DATA  38,228,  0,  0,255, 18,  1, 22,0,1, 62,  0: REM GM127:Applause/Noise
927 DATA   0,  0,  0,  0,243,246,240,201,0,2, 62,  0: REM GM128:Gunshot
928 DATA  16, 17, 68,  0,248,243,119,  6,2,0, 56, 35: REM GP35:Ac Bass Drum
929 DATA  16, 17, 68,  0,248,243,119,  6,2,0, 56, 35: REM GP36:Bass Drum 1
930 DATA   2, 17,  7,  0,249,248,255,255,0,0, 56, 52: REM GP37:Side Stick
931 DATA   0,  0,  0,  0,252,250,  5, 23,2,0, 62, 48: REM GP38:Acoustic Snare
932 DATA   0,  1,  2,  0,255,255,  7,  8,0,0, 48, 58: REM GP39:Hand Clap
933 DATA   0,  0,  0,  0,252,250,  5, 23,2,0, 62, 60: REM GP40:Electric Snare
934 DATA   0,  0,  0,  0,246,246, 12,  6,0,0, 52, 47: REM GP41:Low Floor Tom
935 DATA  12, 18,  0,  0,246,251,  8, 71,0,2, 58, 43: REM GP42:Closed High Hat
936 DATA   0,  0,  0,  0,246,246, 12,  6,0,0, 52, 49: REM GP43:High Floor Tom
937 DATA  12, 18,  0,  5,246,123,  8, 71,0,2, 58, 43: REM GP44:Pedal High Hat
938 DATA   0,  0,  0,  0,246,246, 12,  6,0,0, 52, 51: REM GP45:Low Tom
939 DATA  12, 18,  0,  0,246,203,  2, 67,0,2, 58, 43: REM GP46:Open High Hat
940 DATA   0,  0,  0,  0,246,246, 12,  6,0,0, 52, 54: REM GP47:Low-Mid Tom
941 DATA   0,  0,  0,  0,246,246, 12,  6,0,0, 52, 57: REM GP48:High-Mid Tom
942 DATA  14,208,  0,  0,246,159,  0,  2,0,3, 62, 72: REM GP49:Crash Cymbal 1
943 DATA   0,  0,  0,  0,246,246, 12,  6,0,0, 52, 60: REM GP50:High Tom
944 DATA  14,  7,  8, 74,248,244, 66,228,0,3, 62, 76: REM GP51:Ride Cymbal 1
945 DATA  14,208,  0, 10,245,159, 48,  2,0,0, 62, 84: REM GP52:Chinese Cymbal
946 DATA  14,  7, 10, 93,228,245,228,229,3,1, 54, 36: REM GP53:Ride Bell
947 DATA   2,  5,  3, 10,180,151,  4,247,0,0, 62, 65: REM GP54:Tambourine
948 DATA  78,158,  0,  0,246,159,  0,  2,0,3, 62, 84: REM GP55:Splash Cymbal
949 DATA  17, 16, 69,  8,248,243, 55,  5,2,0, 56, 83: REM GP56:Cow Bell
950 DATA  14,208,  0,  0,246,159,  0,  2,0,3, 62, 84: REM GP57:Crash Cymbal 2
951 DATA 128, 16,  0, 13,255,255,  3, 20,3,0, 60, 24: REM GP58:Vibraslap
952 DATA  14,  7,  8, 74,248,244, 66,228,0,3, 62, 77: REM GP59:Ride Cymbal 2
953 DATA   6,  2, 11,  0,245,245, 12,  8,0,0, 54, 60: REM GP60:High Bongo
954 DATA   1,  2,  0,  0,250,200,191,151,0,0, 55, 65: REM GP61:Low Bongo
955 DATA   1,  1, 81,  0,250,250,135,183,0,0, 54, 59: REM GP62:Mute High Conga
956 DATA   1,  2, 84,  0,250,248,141,184,0,0, 54, 51: REM GP63:Open High Conga
957 DATA   1,  2, 89,  0,250,248,136,182,0,0, 54, 45: REM GP64:Low Conga
958 DATA   1,  0,  0,  0,249,250, 10,  6,3,0, 62, 71: REM GP65:High Timbale
959 DATA   0,  0,128,  0,249,246,137,108,3,0, 62, 60: REM GP66:Low Timbale
960 DATA   3, 12,128,  8,248,246,136,182,3,0, 63, 58: REM GP67:High Agogo
961 DATA   3, 12,133,  0,248,246,136,182,3,0, 63, 53: REM GP68:Low Agogo
962 DATA  14,  0, 64,  8,118,119, 79, 24,0,2, 62, 64: REM GP69:Cabasa
963 DATA  14,  3, 64,  0,200,155, 73,105,0,2, 62, 71: REM GP70:Maracas
964 DATA 215,199,220,  0,173,141,  5,  5,3,0, 62, 61: REM GP71:Short Whistle
965 DATA 215,199,220,  0,168,136,  4,  4,3,0, 62, 61: REM GP72:Long Whistle
966 DATA 128, 17,  0,  0,246,103,  6, 23,3,3, 62, 44: REM GP73:Short Guiro
967 DATA 128, 17,  0,  9,245, 70,  5, 22,2,3, 62, 40: REM GP74:Long Guiro
968 DATA   6, 21, 63,  0,  0,247,244,245,0,0, 49, 69: REM GP75:Claves
969 DATA   6, 18, 63,  0,  0,247,244,245,3,0, 48, 68: REM GP76:High Wood Block
970 DATA   6, 18, 63,  0,  0,247,244,245,0,0, 49, 63: REM GP77:Low Wood Block
971 DATA   1,  2, 88,  0,103,117,231,  7,0,0, 48, 74: REM GP78:Mute Cuica
972 DATA  65, 66, 69,  8,248,117, 72,  5,0,0, 48, 60: REM GP79:Open Cuica
973 DATA  10, 30, 64, 78,224,255,240,  5,3,0, 56, 80: REM GP80:Mute Triangle
974 DATA  10, 30,124, 82,224,255,240,  2,3,0, 56, 64: REM GP81:Open Triangle
975 DATA  14,  0, 64,  8,122,123, 74, 27,0,2, 62, 72: REM GP82
976 DATA  14,  7, 10, 64,228, 85,228, 57,3,1, 54, 73: REM GP83
977 DATA   5,  4,  5, 64,249,214, 50,165,3,0, 62, 70: REM GP84
978 DATA   2, 21, 63,  0,  0,247,243,245,3,0, 56, 68: REM GP85
979 DATA   1,  2, 79,  0,250,248,141,181,0,0, 55, 48: REM GP86
980 DATA   0,  0,  0,  0,246,246, 12,  6,0,0, 52, 53: REM GP87
