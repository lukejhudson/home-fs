// Simple example: one process, showing a while loop and if statement
// Try "spin -p -g -w inc.pml" to view a trace

int x=0;
init {
	do :: true ->
		if :: (x<200) ->
			x=x+1
		fi
	od
}
