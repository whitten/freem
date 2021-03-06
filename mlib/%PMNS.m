%PMNS	; Portable M Namespace Management
	; Copyright (C) 2014 Coherent Logic Development LLC
	QUIT
	;
	;
ADD(NAME)
	N INDEX,RETVAL,BASEPATH,FULLPATH
	S BASEPATH=$V(6),$P(BASEPATH,"/",$L(BASEPATH,"/"))
	S INDEX=$O(^%SYS("NAMESPACE",""),-1)+1
	S RETVAL=1
	S:'$$DIREXIST^%PMFS(BASEPATH) RETVAL=$$MKDIR^%PMFS(BASEPATH)
	G:RETVAL=0 AEDIR
	G:'$$NAMECHK(NAME) AENAME
	S FULLPATH=BASEPATH_"/"_NAME
	G:$$DIREXIST^%PMFS(FULLPATH) AEEXIST
	S RETVAL=$$MKDIR^%PMFS(FULLPATH)
	G:'RETVAL AECREAT
	S RETVAL=$$MKDIR^%PMFS(FULLPATH_"/global")
	S RETVAL=$$MKDIR^%PMFS(FULLPATH_"/routine")
	S:$G(^%SYS("NSAMESPACE"))="" ^%SYS("NAMESPACE")=INDEX
	S ^%SYS("NAMESPACE",INDEX)=NAME
	Q 1
AEDIR	W "Unable to create directory ",BASEPATH,!,! Q 0
AENAME 	W "Invalid characters in '",NAME,"'",!
	W " Valid characters are A-Z, a-z, 0-9, and underscore.",!,! Q 0
AEEXIST W "Namespace already exists.",!,! Q 0
AECREAT	W "Unable to create "_FULLPATH,!,! Q 0
	QUIT
	;
	;
REMOVE(INDEX)
	QUIT 0
	;
	;
SWITCH(INDEX)
	Q:INDEX'=+INDEX 0	; INDEX must be numeric
	N NAME,RETVAL,BASEPATH,FULLPATH
	S BASEPATH=$V(6),$P(BASEPATH,"/",$L(BASEPATH,"/"))="namespace"
	S NAME=$G(^%SYS("NAMESPACE",INDEX))	
	S FULLPATH=BASEPATH_"/"_NAME
	S RETVAL=$$DIREXIST^%PMFS(FULLPATH_"/routine")
	Q:'RETVAL 0
	S RETVAL=$$DIREXIST^%PMFS(FULLPATH_"/global")
	Q:'RETVAL 0
	V 3:FULLPATH_"/global"
	V 4:FULLPATH_"/routine"
	V 5:FULLPATH_"/routine"	
	V 200:INDEX
	QUIT 1
	;
	;
DEFAULT()
	N RETVAL
	S RETVAL=$G(^%SYS("NAMESPACE"))
	I 'RETVAL D
	. D ^%PMSETUP 
	. D CLRSCR^%PMTERM
	. W !,!,$ZVERSION," - Setup Complete",!,!,!
 	. HALT
	Q RETVAL
	;
	;
NAMEOK(NAME)
	S NAME=$TR(NAME,"abcdefghijklmnopqrstuvwxyz")
	S NAME=$TR(NAME,"ABCDEFGHIJKLMNOPQRSTUVWXYZ")
	S NAME=$TR(NAME,"01234567890_")
	Q:NAME="" 1
	Q 0
