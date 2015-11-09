;loader2w.asm
; use this loader for Watcom-32bit-DOS clients rather than loader2.asm!
;
;universal application loader
;
;does the same thing as batch file {mypath}myprog.BAT with following lines:
;
; IF .%DOS4G%==. SET DOS4G=QUIET
; {mypath}RAIN.EXE -q {mypath}myprog.EXE %1 %2 %3 %4 %5 %6 %7 %8 %9
; IF execution failed
;    print appropriate error message
;    errorlevel=99
; IF ERRORLEVEL==99 ;(99=app wasn't run due to some error)
;    echo any key = continue without sound, Ctrl-C = quit
;    pause
; IF ERRORLEVEL==98 or 99 ;(98=rain detected no soundcard)
;    {mypath}myprog.EXE %1 %2 %3 %4 %5 %6 %7 %8 %9
;    IF execution failed
;       print appropriate error message
;       errorlevel=99
; undo that sets
; return errorlevel
;
;RAIN documentation explains sense of this program

.386
.model tiny
cra segment use16
assume es:cra,cs:cra,ss:cra,ds:cra
org 100h
all:

;-----------------------------------CODE---------------------------------------

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

;save offset of ' ', it must be set to 0 before second exec
mov beyondexe,di

;replace our .com filename in rainpath with rain.exe
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

;first exec (with rain)
push cs
pop ds
push ss
pop es
mov ax,4b00h
mov bx,sp
mov dx,offset rainpath
mov executed,dx
mov _seg,ds
int 21h
mov ds,cs:_seg
mov ss,_seg
mov sp,0fffeh
call print_error_fill_al

;optional second exec (without rain)
cmp al,98
je do_exec2
cmp al,99
jne skip_exec2

 ;wait for anykey
 mov ah,9
 mov dx,offset anykey
 int 21h
 mov ah,8
 int 21h
 mov ah,9
 mov dx,offset endl
 int 21h

do_exec2:

 ;fill epb
 push cs                ;inherited fcb 2
 push 6ch
 push cs                ;inherited fcb 1
 push 5ch
 push cs                ;inherited commandline parameters
 push 80h
 mov ax,cs
 add ax,newenvironment16
 push ax                ;newenvironment

 ;second exec
 push cs
 pop ds
 push ss
 pop es
 mov ax,4b00h
  mov bx,beyondexe
  dec bx
  mov word ptr [bx],'$'*256
 mov bx,sp
 mov dx,offset newcommandlinefree
 mov executed,dx
 int 21h
 mov ds,cs:_seg
 mov ss,_seg
 mov sp,0fffeh
 call print_error_fill_al

skip_exec2:

;terminate, expects al=exit code
exit:
mov ah,4ch
int 21h

;--------------------------------SUBROUTINES-----------------------------------

;print error message, put exit code to al
print_error_fill_al:
 push cs
 pop ds
 jnc noerr
 ;child was not executed, print error
 mov ah,9
 cmp al,2
 je notfound
  push ax
  mov dx,offset execerror
  int 21h
  pop ax
 notfound:
  push ax
  mov dx,executed
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
  mov al,99 ;execution failed, set exitcode 99
 ret
 ;child was executed, read exit code
 noerr:
  mov ah,4dh
  int 21h
 ret

;-----------------------------------DATA---------------------------------------

execnotfound         db 'not found.$'
execerror            db 'Error executing $'
endl                 db 13,10,'$'
anykey               db 'any key = continue without sound, Ctrl-C = quit$'
addtoenvlength       = 11+4
addtoenv             db 'DOS4G=QUIET',0,0,1,0
rainfilenamelength   = 10
rainfilename         db 'RAIN.EXE',0,'$'
newcommandlinelength db 0
newcommandline       db '-q '
newcommandlinefree   db 100h dup(?)
_seg                 dw ?
executed             dw ?
beyondexe            dw ?
rainpath             db 100h dup(?)
newenvironmentspace  db ?
newenvironment16     = 50h ;(offset newenvironmentspace+15)/16 at least
newenvironment1      = newenvironment16*16

cra ends
end all