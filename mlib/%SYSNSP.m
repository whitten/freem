%SYSNSP ; A.Trocha ; Namespace Utils [1999/02/01-19:53:19]
	; $Source: /cvsroot-fuse/gump/FreeM/mlib/%SYSNSP.m,v $
	; $Revision: 1.2 $ $Date: 2000/02/18 15:13:42 $
	G show
	Q
show	;--- show namespaces
	; <%ta> tabulator ... if not set it is set to 0
	S %ta=+$G(%ta)
	N idx,dflt
	S idx=""
	S dflt=$G(^%SYS("NSPACE"))
	W !,?%ta,"index     namespace"
	W !,?%ta,"-----  ---------------"
	F  S idx=$O(^%SYS("NSPACE",idx)) Q:idx=""  D
	. W !,?%ta,$J(idx,4),"   ",$S(dflt=idx:"*",1:" ")
	. W ^%SYS("NSPACE",idx)
	W !,?%ta,"-----  ---------------"
	W !
	Q
	;
add()	;--- create new namespace
	; <%ta> tabulator... if not def set to 0
	; results: 1 success
	;          0 error, canceled
	S %ta=+$G(%ta)
	N name,idx,ok,ns,nns
	S ns=$V(6),$P(ns,"/",$L(ns,"/"))="namespace"
	S idx=$O(^%SYS("NSPACE",""),-1)+1
	;
	S ok=1
	I '$$direx^%FUTIL(ns) S ok=$$mkdir^%FUTIL(ns)
	I ok=0 G adderr0
add0	W !,?%ta,"Name ..........: " R name I name="" Q 0
	I $$nsok(name)=0 D adderr1 G add0
	I $$direx^%FUTIL(ns_"/"_name) D adderr2 G add0
	W !,?%ta,"Ok to create? <Y> "
	I $$ask^%MUTIL("Y") G add1
add2	W !,?%ta,"Do you want to try again? <Y> "
	I $$ask^%MUTIL("Y") W !! G add0
	Q 0
add1	;
	;--- create namespace
	S nns=ns_"/"_name
	S ok=$$mkdir^%FUTIL(nns) I 'ok D adderr3 G add2
	S ok=$$mkdir^%FUTIL(nns_"/global")
	S ok=$$mkdir^%FUTIL(nns_"/routine")
	I $G(^%SYS("NSPACE"))="" S ^%SYS("NSPACE")=idx
	S ^%SYS("NSPACE",idx)=name
	W !,?%ta,"Done.",!
	Q 1
adderr0	W !,?%ta,"ERROR unable to create directory "_ns,!! Q 0
adderr1 W !,?%ta,"ERROR invalid namespace. You may choose following characters"
	W !,?%ta,"      A..Z, a..z, 0..9, _",!! Q
adderr2 W !,?%ta,"ERROR namespace already exists! Please chose another name!",!! Q
adderr3	W !,?%ta,"ERROR unable to create "_nns,!! Q
	;
dflt()	;--- get the default namespace
	; <%ta> tabulator ... if not def it is set to 0
	S %ta=+$G(%ta)
	N dflt,idx,erg,quit
	S quit=0
	S dflt=$G(^%SYS("NSPACE"))
	;
	;--- in case a default namespace is defined - return the index
	I dflt=+dflt Q dflt
	;
	;--- seems to be no namespace defined 
	I dflt="" D
	. ;
	. ;--- better check if there is any namespace defined
	. S idx=$O(^%SYS("NSPACE",""),-1)
	. ;
	. ;--- 
	. I idx="" D
	. . ;
	. . ;--- oops, better create a namespace
	. . W !,?%ta,"Seems that there is no namespace defined in your System!"
	. . W !,?%ta,"Do you want to create a namespace? <Y> "
	. . I $$ask^%MUTIL("Y") W ! S erg=$$add() S quit='erg Q
	. . S quit=1
	. Q:quit
	. I idx'="" S ^%SYS("NSPACE")=idx Q
	. S dflt=$G(^%SYS("NSPACE")) I dflt="" S quit=1
	Q:quit 0
	Q dflt
	;
switch(i) ;--- switch to given namespace
	; result:  1 =ok     0=error
	Q:i'=+i 0
	N name,ok,ns,nns
	S ns=$V(6),$P(ns,"/",$L(ns,"/"))="namespace"
	S name=$G(^%SYS("NSPACE",i))
	;
	;--- test if directories exist
	S nns=ns_"/"_name
	S ok=$$direx^%FUTIL(nns_"/routine")
	I 'ok Q 0
	S ok=$$direx^%FUTIL(nns_"/global")
	I 'ok Q 0
	;
	;--- change directory pointers
	V 3:nns_"/global"
	V 4:nns_"/routine"
	V 5:nns_"/routine"
	V 200:i
	Q 1
	;
nsok(s)	;--- namespace convertions
	S s=$TR(s,"abcdefghijklmnopqrstuvwxyz")
	S s=$TR(s,"ABCDEFGHIJKLMNOPQRSTUVWXYZ")
	S s=$TR(s,"01234567890_")
	I s="" Q 1
	Q 0
