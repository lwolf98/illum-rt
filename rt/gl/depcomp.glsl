checked=""

Is[0]="."
N=1
while [ "$1" == "-I" ] ; do
	echo "[$1 $2]"
	Is[$N]="$2"
	N=$((N+1))
	shift 2
done

function check() {
	checked="$1 $checked"
	input="$1"
	for i in $(seq 0 $N) ; do
		if [ -e "${Is[$i]}/$1" ] ; then
			input="${Is[$i]}/$1"
		fi
	done
	direct=$(grep 'include(' $input | sed -e 's/.*include(\([^)]*\)).*/\1/g');
	for g in $direct ; do
		echo -n " $g"
	done
	echo
	for g in $direct ; do
		echo "$checked" | grep $g > /dev/null
		if [ "$?" == "1" ] ; then
			check $g
		fi
	done
}

for f in $* ; do
	echo -n "$(echo $f | sed -e 's/\.glsl/-glsl.cpp/'):"
	check $f
done
