mtype = { coin, press1, press2, press3, serve };

int drink = 0;
chan c = [0] of { mtype };
int c1coins = 5;
int c2coins = 5;

proctype Machine() {
	do :: true -> 
		c?coin ; 
		if
		:: c?press1 -> drink1 : drink = 1
		:: c?press2 -> drink = 2
		:: c?press3 -> drink = 2
		:: c?press3 -> drink = 3
		fi ;
		chosen: c!serve
	od
}
proctype Customer() {
	do :: c1coins > 0 ->
		insertCoin1: c!coin ;
		c!press1 ;
		c?serve
		c1coins-- ;
	od
}
proctype Customer2() {
	do :: c2coins > 0 ->
		insertCoin2: c!coin ;
		if
		:: true -> c!press2
		:: true -> c!press3
		:: true -> c1coins++
		fi ;
		c?serve
		c2coins-- ;
	od
}
init { 
	run Machine() ; run Customer() ; run Customer2()
} 

#define a (Machine@chosen)
#define b (Machine@drink1)
#define c (Customer@insertCoin1)
#define d (Customer2@insertCoin2)

ltl prop1 {! <> a }
ltl q1d {[] <> a}
ltl q2a {[] <> b}
ltl q2b {![] <> b}
ltl q3a {!(([] <> c) && ([] <> d))}
ltl q3b {! <> (c1coins==10)}



















