%SYSGEN	; A.Trocha ; System Setup [1999/02/01- 0:06:41]
	; $Source: /cvsroot-fuse/gump/FreeM/mlib/%SYSGEN.m,v $
	; $Revision: 1.2 $ $Date: 2000/02/18 15:13:42 $
	;
	N keys,x,menu,path,tmp
	S menu="mroot",path="mroot"
	S %ta=8
	W !!!!,?%ta,"FreeM SYSTEM SETUP UTILITY"
	W !,?(%ta+3),$ZD,"  ",$ZT
again	K keys
	D showmenu(menu)
	W !
enter	W !,?%ta,"Your choice: " R x 
	I x=""&(path="mroot") W !! Q
	I x="" S path=$P(path,"-",2,99),menu=$P(path,"-") G again
	S tmp=$G(keys(x))
	I tmp="" G enter
	I tmp'="" D
	. I $E(tmp)="m" S menu=tmp,path=menu_"-"_path Q
	. X tmp
	G again 
	Q
	;
showmenu(labl) ;--- show sysgen menu
	N i,key
	W !!!
	F i=0:1 S line=$T(@labl+@i) Q:line=""!($F(line,";*;"))  D
	. S key=$P(line,";",2)
	. S param=$P(line,";",3)
	. S param2=$P(line,";",4)
	. I key="i",param'="" D
	. . W !,?%ta,param,!,?%ta,$TR($J("",$L(param))," ","-"),!
	. I key="i"&(param2'="") X param2 W !
	. I key="i" Q
	. W !,?%ta,key,"  -  ",param
	. S keys(key)=$P(line,";",4)
	Q
	;
	;---------------------------------------------------------------------
mroot	;i;Main Menu
	;1;Namespace Settings;mnamesp
	;2;Routine-Editor Settings;mred
	;3;Configuration Settings
	;4;Startup Options;mstart
	;*;
mnamesp	;i;Namespace Settings
	;1;List Namespaces;D show^%SYSNSP
	;2;add Namespace;S err=$$add^%SYSNSP
	;3;delete Namespace;D del^%SYSNSP0
	;4;rename Namespace
	;5;repair Namespace;D rep^%SYSNSP0 R x
	;*;
mred	;i;Routine-Editor Settings;D current^%ED
	;1;choose another editor;D other^%ED
	;*;
mstart	;i;Startup Options
	;1;none
	;*;
