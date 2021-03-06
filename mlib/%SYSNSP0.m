%SYSNSP0 ; A.Trocha ; Namespace Utils [1999/02/01-19:53:19]
	; $Source: /cvsroot-fuse/gump/FreeM/mlib/%SYSNSP0.m,v $
	; $Revision: 1.2 $ $Date: 2000/02/18 15:13:42 $
	Q
	;
del	;--- delete a namespace
	N x,erg,ns
	S ns=$V(6),$P(ns,"/",$L(ns,"/"))="namespace"
	S %ta=+$G(%ta)
del0	W !!,?%ta,"Which namespace to delete? "
	R x I x="" W ! Q
	I '$$direx^%FUTIL(ns_"/"_x) D del1 G del0
	W !,?%ta,"Sure ? <N> " S erg=$$ask^%MUTIL("N")
	I erg=1 G del0
	;
	;--- loesche directory
	S erg=$$rmdir^%FUTIL(ns_"/"_x)
	;
	;--- loesche Eintrag aus ^%SYS
	S idx=$$nsidx(x)
	I idx>0 D
	. K ^%SYS("NSPACE",idx)
	. I idx=+$G(^%SYS("NSPACE")) D setdflt
	;
	I erg W !,?%ta,"Successfully deleted!"
	E  W !,?%ta,"Failed."
	Q
del1	W !,?%ta,*7,"ERROR This namespace does not exist!" G del0
	;
	;
rep	;--- repair namespace
	N idx,name,tab,exec,%,rtn,glo,nidx,erg,dflt,ns
	S ns=$V(6),$P(ns,"/",$L(ns,"/"))="namespace"
	S %ta=+$G(%ta)
	S idx=""
	W !,?(%ta),"FIRST PASS:",!
	F  S idx=$O(^%SYS("NSPACE",idx)) Q:idx=""  D
	. W !,?%ta,$J(idx,2),") "
	. S name=^%SYS("NSPACE",idx)
	. W name,":    "
	. I $D(tab(name)) D repDUPE Q
	. I '$$direx^%FUTIL(ns_"/"_name) D repNDIR Q
	. S tab(name)=1
	. W "ok"
	W !!,?(%ta),"SECOND PASS:",!
	S exec="!<ls "_ns_" 2>/dev/null"
	X exec
	S idx=""
	F  S idx=$O(%(idx)) Q:idx=""  D
	. W !,?(%ta),$J(idx,2),") "
	. S name=%(idx)
	. W name,":   "
	. I '$$direx^%FUTIL(ns_"/"_name) D repNDI Q
	. S glo=$$direx^%FUTIL(ns_"/"_name_"/global")
	. S rtn=$$direx^%FUTIL(ns_"/"_name_"/routine")
	. I glo=0,rtn=0 D repEMP Q
	. I rtn=0 D repMR
	. I glo=0 D repMG
	. ;
	. I $D(tab(name)) W "  ok." Q
	. S nidx=$O(^%SYS("NSPACE",""),-1)+1
	. S ^%SYS("NSPACE",nidx)=name
	. W "  ^%SYS entry added - ok."
	S dflt=$G(^%SYS("NSPACE"))
	I dflt'=""&('$D(^%SYS("NSPACE",dflt))) D setdlft
	W !,?%ta,"Done.",!
	Q
repDUPE	W "dupe  =>  killed." K ^%SYS("NSPACE",idx) Q
repNDIR	W "no corresponding dir found  =>  killed." K ^%SYS("NSPACE",idx) Q
repNDI	W "that is no directory  =>  ignoring" Q
repEMP	W "empty => deleting." S erg=$$rmdir^%FUTIL(ns_"/"_name) Q
repMG	W "mkdir global."
	S erg=$$mkdir^%FUTIL(ns_"/"_name_"/global") Q
repMR	W "mkdir routine."
	S erg=$$mkdir^%FUTIL(ns_"/"_name_"/routine") Q
	;
	;
nsidx(name) ;---get corresponding index
	N i,found
	S i="",found=0
	F  S i=$O(^%SYS("NSPACE",i)) Q:i=""  D  Q:found
	. I ^%SYS("NSPACE",i)=name S found=1
	I found Q i
	Q -1

setdflt	;--- automatically assign new default namespace
	N idx
	S idx=$O(^%SYS("NSPACE",""))
	I idx'="" S ^%SYS("NSPACE")=idx
	I idx="" K ^%SYS("NSPACE")
	Q
