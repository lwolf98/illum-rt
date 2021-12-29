checked=""
function check() {
	checked="$1 $checked"
	direct=$(grep 'include(' $1 | sed -e 's/.*include(\([^)]*\)).*/\1/g');
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
