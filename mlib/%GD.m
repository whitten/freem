%GD	; A.Trocha; Global Directory 01/29/1999 00:44/GMT+1
	; $Source: /cvsroot-fuse/gump/FreeM/mlib/%GD.m,v $
	; $Revision: 1.3 $ $Date: 2000/02/18 15:13:41 $
	;
	; this version is very, very beta
	;
	; todo: LOCKING!!
	;
	N gnorm,gsys,xdir,anz
	;
	;--- path to non-% globals
	S gnorm=$V(3)
	;
	;--- path to %-globals
	S gsys=$V(6)
	;
	;
	W !,?20,"GLOBAL DIRECTORY   "_$ZD
	W !,?23,"OF FreeM       "_$ZT
	W !!
	;
	S anz=0
	;
	;--- get and output %-globals
	K %
	S xdir="!<"_$$dircmd()_" "_$$convpath(gsys)_"^* 2>/dev/null"
	S $ZT="error^"_$ZN
	X xdir D show(0)
	;
	;--- get and output non-% globals
	K %
	S xdir="!<"_$$dircmd()_" "_$$convpath(gnorm)_"^* 2>/dev/null"
	X xdir D show(1)
	;
end	W !,$J(anz,8)," - Globals",!!
	K %
	Q
	;
show(m)	;--- show globals
	;    m=0 show %global  ;   m=1 show non% globals
	;    do not output ^$<xxxxxx>
	N i
	F i=1:1:% D
	. I $G(m)=0&('$F($G(%(i)),"%")) Q
	. I $G(m)=1&($F($G(%(i)),"%")) Q
	. I $F($G(%(i)),"$") Q
	. W $$lb("^"_$P($G(%(i)),"^",2)) S anz=anz+1
	Q
	;
convpath(dir) ;--- convert path
	N sl
	S sl=$$slash()
	I dir="" Q ""
	I dir="."_sl Q ""
	I $E(dir,$L(dir))'=sl Q dir_sl
	Q dir
	;
lb(str)	;---
	Q $E(str,1,9)_$J("",10-$L(str))
	;
slash() ;--- get the OS specific directory delimiter (slash)
	Q "/"
	;
dircmd() ;--
	;--- get the OS specific directory command
	;--- hmm!? how do I know which OS?
	Q "ls"
	;
error	;--- error - trap
	W !,$ZE,!!
	K % Q
