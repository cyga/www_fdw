#!/bin/bash
test_dir=`echo $0 | perl -pe's#(.*)/.*#$1#;'`

while [ $# -gt 0 ]; do
    case "$1" in
        --)     shift; break 2;;
        -a)		all=1; shift;;
        --all)	all=1; shift;;
        --json)	json=1; shift;;
        --xml)	xml=1; shift;;
        *)      break;;
    esac
    shift
done

if [ -n "$all" ]; then
	xml=1
	json=1
fi

trap 'let fall=$fsuccess+$ffailed;echo;echo "Tests files run: $fall";echo "Ok: $fsuccess";echo "Failed: $ffailed"; if [ 0 -lt "$ffailed" ]; then exit 1; fi' EXIT

fsuccess=0
ffailed=0
for t in $test_dir/test-*.sh; do
	if [[ "$0" == "$t" ]]; then
		echo "skipping itself: '$t'"
		continue;
	fi

	if [[ "$test_dir/test-json-response-deserialize-callback.sh" == "$t" && -z "$json" ]]; then
		echo "need json module for this callback (--json or -a|--all option)";
		continue;
	fi

	if [[ "$test_dir/test-xml-response-deserialize-callback.sh" == "$t" && -z "$xml" ]]; then
		echo "need xml support in postgres for this callback (--xml or -a|--all option)";
		continue;
	fi

	echo "############## test: '$t' ##############"
	"$t"
	if [ 0 == "$?" ]; then
		let fsuccess=$fsuccess+1
	else
		let ffailed=$ffailed+1
	fi
	echo ''
done
