{$A+,B-,D-,F-,I+,L+,O-,P-,Q-,R-,S-,T+,V+,X+}

{$ifdef VER70}                 { Borland Pascal }
 {$ifdef windows}
 Uses      Rain,WinCrt;
 {$else}
 Uses      Rain;{avoid Borland's Crt crashing on fast computers}
 Function  KeyPressed:boolean; inline($b4/$0b/$cd/$21);{mov ah,0bh;int 21h}
 Function  ReadKey:char;       inline($b4/$08/$cd/$21);{mov ah,8;int 21h}
 {$endif}
{$endif}

{$ifdef FPC}                    { FreePascal }
 {$mode tp}
 Uses      Rain,Crt;
{$endif}

{$ifdef __TMT__}                { TMT Pascal }
 Uses      Rain,Crt;
{$endif}

Var       ourSoundHandle:longint;
Const     ourSoundEnded:boolean=false;

Procedure OurReportEnd(handle:longint);far;
begin
 if handle=ourSoundHandle then ourSoundEnded:=true;
end;

BEGIN
 {initialize RAIN and write name of server}
 writeln(RainInit);

 {do we have anything to play?}
 if paramcount=0 then begin writeln('No soundfile to play.');halt(1);end;

 {hook our function that will be called each time some sound ends}
 ReportEnd:=OurReportEnd;

 {start playing sound}
 ourSoundHandle:=RainPlay(paramstr(1),0,0,0{no loop},64{volume},0{panning});

 {repeat until sound end or user abort}
 repeat
   RainPoll;
 until ourSoundEnded or KeyPressed;

 {clear keypresses}
 while KeyPressed do ReadKey;
END.
