;loader1w.asm
; use this loader for Watcom-32bit-DOS clients rather than loader1.asm!
;
;old universal application loader
; tries to run app with rain, doesn't retry without rain as new one
; use in cases where running without sound has no meaning
;
;does the same thing as batch file X:\PATH\MYPROG.BAT with following lines:
; IF .%DOS4G%==. SET DOS4G=QUIET
; X:\PATH\RAIN.EXE -q X:\PATH\MYPROG.EXE %1 %2 %3 %4 %5 %6 %7 %8 %9
; undo that sets
;RAIN documentation explains sense of this program

.386
.model tiny
cra segment use16
assume es:cra,cs:cra,ss:cra,ds:cra
org 100h
all:

;copy environment to newenvironment
;search our filename to ds:si
mov ds,ds:[2ch]
xor ax,ax
xor si,si
mov di,newenvironment1
a:
 cmp [si],ax
 movsb
 jnz a
add si,3

;append my vars to the end of newenvironment
PUSH SI
mov si,offset addtoenv
mov cx,addtoenvlength
segcs rep movsb
POP SI

;shrink memory block
add di,15+100h
shr di,4
mov bx,di
shl di,4
mov sp,di
mov ah,4ah
int 21h

;copy our filename to new commandline and to rainpath
mov di,offset newcommandlinefree
b:
 lodsb
 mov es:[di+offset rainpath-offset newcommandlinefree],al
 stosb
 or al,al
 jnz b

;replace our .com extension in new commandline with client's .exe extension
push es
pop ds
mov dword ptr [di-4],' EXE'

;replace our .com filename in rainpath with rain-win.exe
PUSH DI ;preserve di=end of new commandline
add di,offset rainpath-offset newcommandlinefree
c:
 dec di
 cmp byte ptr [di],'\'
 jnz c
inc di
mov si,offset rainfilename
mov cx,rainfilenamelength
rep movsb
POP DI

;append our commandline to new commandline
mov si,80h
xchg ax,cx
lodsb
cbw
xchg cx,ax
rep movsb

;mark end of commandline with 0dh, required by NT
mov word ptr ds:[di],0dh

;set first commandline byte - its length
xchg ax,di
sub ax,offset newcommandline
mov newcommandlinelength,al

;fill epb
push cs                ;inherited fcb 2
push 6ch
push cs                ;inherited fcb 1
push 5ch
push cs                ;modified commandline parameters (first one inserted)
push offset newcommandlinelength
mov ax,cs
add ax,newenvironment16
push ax                ;newenvironment

;exec
push cs
pop ds
push ss
pop es
mov ax,4b00h
mov bx,sp
mov dx,offset rainpath
mov _seg,ds
int 21h
mov ds,cs:_seg
mov ss,_seg
mov sp,0fffeh

;print error message
jnc noerr
push cs
pop ds
mov ah,9
cmp al,2
je notfound
 push ax
 mov dx,offset execerror
 int 21h
 pop ax
notfound:
 push ax
 mov dx,offset rainpath
 int 21h
 pop ax
cmp al,2
jne found
 push ax
 mov dx,offset execnotfound
 int 21h
 pop ax
found:
 mov dx,offset endl
 int 21h
 mov al,99 ;execution failed, return exitcode 99
 jmp exit
noerr:

;propagate exit status
mov ah,4dh
int 21h

;terminate
exit:
mov ah,4ch
int 21h

execnotfound         db 'not found$'
execerror            db 'Error executing $'
endl                 db 13,10,'$'
addtoenvlength       = 11+4
addtoenv             db 'DOS4G=QUIET',0,0,1,0
rainfilenamelength   = 10
rainfilename         db 'RAIN.EXE',0,'$'
newcommandlinelength db 0
newcommandline       db '-q '
newcommandlinefree   db 100h dup(?)
_seg                 dw ?
rainpath             db 100h dup(?)
newenvironmentspace  db ?
newenvironment16     = 41h ;(offset newenvironmentspace+15)/16
newenvironment1      = newenvironment16*16

cra ends
end all