{ RAIN high-level Pascal API + implementation

  tested compilers/targets:
  - BP 7.01 DOS 16bit real mode
  - BP 7.01 DOS 16bit protected mode
  - BP 7.01 Windows 16bit protected mode
  - FreePascal 1.0.4 DOS 32bit protected mode (GO32V2)
  - FreePascal 1.0.4 Windows 32bit protected mode
  - TMT Pascal 3.50 DOS 32bit protected mode
}

Unit      Rain;

 { Borland Pascal }
 {$ifdef VER70}
 {$B-,E-,G+,I-,P-,Q-,R-,S-,T+,V+,X+}
 {$endif}

 { FreePascal }
 {$ifdef FPC}
 {$B-,E-,G+,I-,P-,Q-,R-,S-,T+,V+,X+}
 {$ASMMODE Intel}
 {$LONGSTRINGS off}
 {$MACRO on}
 {$MODE TP}
 {$endif}

 { TMT Pascal }
 {$ifdef __TMT__}
 {$B-,I-,P-,Q-,R-,T+,X+}
 {$OPT+}
 {$endif}

Interface

Type      TReportError=procedure(s:string);
Type      TReportEnd=procedure(handle:longint);
Var       ReportError:TReportError;
Var       ReportEnd:TReportEnd;

FUNCTION  RainInit:string;
PROCEDURE RainPoll;
FUNCTION  RainPlay(filename:string;fileofs,filesize,loop,volume,panning:longint):longint;
PROCEDURE RainSetVolume(handle,volume:longint);
PROCEDURE RainSetPanning(handle,panning:longint);
PROCEDURE RainStop(handle:longint);
PROCEDURE RainAmplification(amp:longint);

Var       async:byte;

{ßßßßßßßßßßßßßßßßßßßßßßßßßßßßßßßßßßßßßßßßßßßßßßßßßßßßßßßßßßßßßßßßßßßßß}
                          IMPLEMENTATION
{ÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜ}

uses {$ifdef FPC}{$ifdef GO32V2},Dos,{$endif}{$endif}
     {$ifdef __TMT__}{$ifdef __DOS__}DPMI,{$endif}{$endif}
     Strings,Objects;

const
  CMD_NONE     =0;
  CMD_PLAY     =1;
  CMD_STOP     =2;
  CMD_VOL      =3;
  CMD_PAN      =4;
  CMD_AMPLIFY  =5;
  CMD_TERMINATE=-1;

const
  protokol:byte=0;

{protokol 1}
const
  talk_seg:word  =0;    {segment or selector or talk buffer}
  TALK_BUFSIZE   =1024; {talk buffer length}
  TALK_STRSIZE   =256;  {max talk string length including zero end}
  TALK_CMD       =4*0;
  TALK_PARAM1    =4*1;
  TALK_PARAM2    =4*2;
  TALK_PARAM3    =4*3;
  TALK_FILE_OFS  =4*4;
  TALK_FILE_SIZE =4*5;
  TALK_FILE_NAME =4*6;
  TALK_REPORT    =4*128;

{protokol 2}
var
  raincmd:text;
  rainrep:file;
  raincmdname:string;
  rainrepname:string;
const
  lasthandle:longint=0;
var
  input:string;
const
  skipeol:boolean=false;
function ReadReply:string;
  var readen,p10:word;
  begin
   ReadReply:='';
   blockread(rainrep,input[length(input)+1],255-length(input),readen);
   inc(input[0],readen);
   p10:=pos(#10,input);
   if (p10>0) then begin
     if input[p10-1]=#13 then begin delete(input,p10-1,1);dec(p10);end;
     if not skipeol then ReadReply:=copy(input,1,p10-1);
     delete(input,1,p10);
     skipeol:=false;
     end
   else if length(input)=255 then begin
     if not skipeol then ReadReply:=input;
     input:='';
     skipeol:=true;
     end;
  end;

{asynchronous calls}
var
  errors:TStringCollection;
procedure CleverReportError(error:string);
  begin
   if async=0 then ReportError(error) else errors.Insert(NewStr(error));
  end;
procedure SetCol(col,r,g,b:byte);assembler;
  asm
   push	dx
   mov	dx,3c8h
   mov	al,col
   out	dx,al
   inc	dx
   mov	al,r
   out	dx,al
   mov	al,g
   out	dx,al
   mov	al,b
   out	dx,al
   pop	dx
  end;
function DosSafe:boolean;
  const
    dossafe_inited:boolean=false;
    dossafe_ofs:word=0;
    dossafe_seg:word=0;
  begin
  {$ifdef VER70}
    {$ifdef WINDOWS}
    DosSafe:=true;
    {$else}
    if not dossafe_inited then asm
       mov	ah,34h
       int	21h
       mov	word ptr dossafe_ofs,bx
       mov	word ptr dossafe_seg,es
       inc	dossafe_inited
       end;
    DosSafe:=mem[dossafe_seg:dossafe_ofs]=0;
    {$endif}
  {$endif}
  {$ifdef FPC}
    {$ifdef WIN32}
    DosSafe:=true;
    {$else}
    {I wasn't able to test my DosSafe detection code under FPC-GO32V2, so
     let's rather return FALSE for safety. It means that asynchronous calls
     will be ALWAYS rejected until somebody contributes trustworthy code.}
    DosSafe:=false;
    {$endif}
  {$endif}
  {$ifdef __TMT__}
    {$ifdef __WIN32__}
    DosSafe:=true;
    {$else}
    if not dossafe_inited then
     declare
      var regs:trmregs;
     begin
      clearrmregs(regs);
      regs.ah:=$34;
      realmodeint($21,regs);
      dossafe_ofs:=regs.bx;
      dossafe_seg:=regs.es;
      dossafe_inited:=true;
      end;
    DosSafe:=mem[dossafe_seg:dossafe_ofs]=0;
    {$endif}
  {$endif}
  end;

{common}
function Str2Pc(var str:string):Pchar;
  begin
   if length(str)=255 then str[255]:=#0 else str[length(str)+1]:=#0;
   Str2Pc:=@str[1];
  end;
function Talk2Str(talk_ofs:word):string;
  var s:string;
  var i:integer;
  begin
   for i:=0 to 254 do begin
     s[i+1]:=chr(mem[talk_seg:talk_ofs+i]);
     if s[i+1]=#0 then break;
     end;
   s[0]:=chr(i);
   Talk2Str:=s;
  end;
procedure Pc2Talk(p:pchar;talk_ofs:word);
  begin
   repeat
    mem[talk_seg:talk_ofs]:=byte(p^);
    if p^=#0 then break;
    inc(p);
    inc(talk_ofs);
   until false;
  end;

  {$ifdef WINDOWS}
  function GetDOSEnvironment:Pchar;far; external 'kernel' index 131;
  {$endif}

  {$ifdef WIN32}
  function GetEnvironmentStrings:Pchar; external 'kernel32' name 'GetEnvironmentStringsA';
  {$endif}

  {$ifdef fpc}
  var fakeenv:string;
  {$endif}

function GetEnvVar(VarName:Pchar):Pchar;
  {if possible (not in FreePascal-DOS)
   returns pointer into REAL environment, not its copy}
  var
   L: Word;
   P: PChar;
  begin
   L := StrLen(VarName);
   {$IFDEF VER70}
     {$IFDEF WINDOWS}
     P := GetDOSEnvironment;
     {$ELSE}
     P := Ptr(Word(Ptr(PrefixSeg, $2C)^), 0);
     {$ENDIF}
   {$ENDIF}
   {$IFDEF FPC}
     {$IFDEF WIN32}
     P := GetEnvironmentStrings;
     {$ELSE}
     fakeenv:='N='+GetEnv(StrPas(VarName));   {hack for FPC/GO32V2}
     GetEnvVar:=Str2Pc(fakeenv)+2;
     exit;
     {$ENDIF}
   {$ENDIF}
   {$IFDEF __TMT__}
     P := _environ;
   {$ENDIF}
   while P^ <> #0 do
   begin
     if (StrLIComp(P, VarName, L) = 0) and (P[L] = '=') then
     begin
       GetEnvVar := P + L + 1;
       Exit;
     end;
     Inc(P, StrLen(P) + 1);
   end;
   GetEnvVar := nil;
  end;

FUNCTION RainInit:string;
var j,code:word;
    env:pchar;
    ver:string;
    p:^pchar;
label err;
begin
 if protokol<>0 then begin
   RainInit:='RainInit may be called just once.';
   exit;
   end;
 RainInit:='';
 env:=GetEnvVar('RAIN');
 if env<>nil then begin
   ver:=StrPas(env);
   {invalidate rain variable so child processes won't think rain is for them}
   (env-2)^:='X';
   {is rain talking via memory?}
   if copy(ver,1,4)='SEG ' then begin
      val(copy(ver,5,255),talk_seg,code);
      if (code=0) and (talk_seg<>0) then begin
        {$ifdef VER70}
        {$ifdef dpmi}
        asm
          mov	ax,2
          mov	bx,talk_seg
          int	31h
          jc	err
          mov	talk_seg,ax
        end;
        {$endif}
        {$endif}
        RainInit:=Talk2Str(TALK_REPORT);
        mem[TALK_SEG:TALK_REPORT]:=0;
        protokol:=1;
        exit;
        err:
        end;
      end;
   {is rain talking via file?}
   if copy(ver,1,5)='FILE ' then begin
      raincmdname:=copy(ver,6,255);
      j:=pos(',',raincmdname);
      if (j>1) and (j<length(raincmdname)) then begin
        rainrepname:=copy(raincmdname,j+1,255);
        raincmdname[0]:=chr(j-1);
        assign(rainrep,rainrepname);
        filemode:=$40;
        reset(rainrep,1);
        filemode:=2;
        if ioresult=0 then begin
          assign(raincmd,raincmdname);
          filemode:=$41;
          append(raincmd);
          if ioresult<>0 then rewrite(raincmd);
          filemode:=2;
          if ioresult=0 then begin
            input:='';
            repeat ver:=ReadReply until ver<>'';{wait for server version}
            RainInit:=ver;
            repeat ver:=ReadReply until ver='';{skip replies for previous clients}
            protokol:=2;
            exit;
            end;
          close(rainrep);
          end;
        end;
      end;
   end;
end;

FUNCTION RainCommand(command,param1,param2,param3:longint;filename:pchar;fileofs,filesize:longint):longint;
var   reply:string;
      handle:longint;
      code:integer;
      error:pstring;
const raincommands:byte=0;
begin
 RainCommand:=0;

{ if DosSafe then setcol(0,0,0,0) else setcol(0,63,0,0); }

 {just one command at one time, ignore asynchronous commands if necessary}
 inc(raincommands);
 if (raincommands=1) and ((async=0) or DosSafe) then begin

   {report errors stored by asynchronously issued commands}
   if async=0 then
    while errors.count>0 do begin
      error:=errors.At(0);
      errors.AtDelete(0);
      ReportError(error^);
      DisposeStr(error);
      end;
   {send command, read reply}
   case protokol of
     1:begin
       meml[TALK_SEG:TALK_PARAM1]:=param1;
       meml[TALK_SEG:TALK_PARAM2]:=param2;
       meml[TALK_SEG:TALK_PARAM3]:=param3;
       if filename=nil then mem[TALK_SEG:TALK_FILE_NAME]:=0 else Pc2Talk(filename,TALK_FILE_NAME);
       meml[TALK_SEG:TALK_FILE_OFS]:=fileofs;
       meml[TALK_SEG:TALK_FILE_SIZE]:=filesize;
       meml[TALK_SEG:TALK_CMD]:=command;
       {$ifdef VER70}
       asm int 12h;end;
       {$else}
       asm pushad;int 12h;popad;end;
       {$endif}
       reply:=Talk2Str(TALK_REPORT);
       if reply<>'' then mem[TALK_SEG:TALK_REPORT]:=0;
       end;
     2:begin
       case command of
         CMD_PLAY     :writeln(raincmd,command,' ',param1,' ',param2,' ',param3,' ',fileofs,' ',filesize,' ',filename);
         CMD_VOL      ,
         CMD_PAN      :writeln(raincmd,command,' ',param1,' ',param2);
         CMD_STOP     ,
         CMD_AMPLIFY  ,
         CMD_TERMINATE:writeln(raincmd,command,' ',param1);
         end;
       flush(raincmd);
       reply:=ReadReply;
       end;
     else
       reply:='';
     end;
   {return handle if needed}
   if command=CMD_PLAY then begin
     inc(lasthandle);
     RainCommand:=lasthandle;
     end;
   {call reporters}
   if reply<>'' then
    if reply[1]=' ' then CleverReportError(copy(reply,2,255)) else
     if copy(reply,1,7)='end of ' then begin
       val(copy(reply,8,255),handle,code);
       if code=0 then ReportEnd(handle)
         else CleverReportError('Unknown report: '+reply);
       end
     else CleverReportError('Unknown report: '+reply);

   end;
 dec(raincommands);
end;

PROCEDURE RainPoll;
begin
 RainCommand(CMD_NONE,0,0,0,nil,0,0);
end;

FUNCTION  RainPlay(filename:string;fileofs,filesize,loop,volume,panning:longint):longint;
begin
 RainPlay:=RainCommand(CMD_PLAY,loop,volume,panning,Str2Pc(filename),fileofs,filesize);
end;

PROCEDURE RainSetVolume(handle,volume:longint);
begin
 RainCommand(CMD_VOL,handle,volume,0,nil,0,0);
end;

PROCEDURE RainSetPanning(handle,panning:longint);
begin
 RainCommand(CMD_PAN,handle,panning,0,nil,0,0);
end;

PROCEDURE RainStop(handle:longint);
begin
 RainCommand(CMD_STOP,handle,0,0,nil,0,0);
end;

PROCEDURE RainAmplification(amp:longint);
begin
 RainCommand(CMD_AMPLIFY,amp,0,0,nil,0,0);
end;

procedure IgnoreEnd(handle:longint);far;
begin
{ writeln('RAIN: end of ',handle); }
end;

procedure EchoError(s:string);far;
begin
 writeln('RAIN: ',s);
end;


var oldExitProc:pointer;

procedure RainExitProc;far;
 var env:pchar;
     p:^pchar;
 begin
  exitProc:=oldExitProc;
  RainCommand(CMD_TERMINATE,exitcode,0,0,nil,0,0);
  errors.Done;
  {validate rain variable to get rain visible for possible next clients}
  env:=GetEnvVar('RAIX');
  if env<>nil then (env-2)^:='N';
 end;

BEGIN
 ReportError:=EchoError;
 ReportEnd:=IgnoreEnd;
 async:=1;
 errors.Init(5,5);
 oldExitProc:=exitProc;
 exitProc:=@RainExitProc;
END.
