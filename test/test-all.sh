#!/bin/bash
test_dir=`echo $0 | perl -pe's#(.*/).*#$1#;'`

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

for t in \
	test-default-json.sh \
	test-default-xml.sh \
	test-request-serialize-callback.sh \
	test-json-response-deserialize-callback.sh \
	test-xml-response-deserialize-callback.sh \
	test-other-response-deserialize-callback.sh \
	test-response-iterate-callback.sh
do
	if [[ "test-json-response-deserialize-callback.sh" == "$t" && -z "$json" ]]; then
		echo "need json module for this callback (--json or -a|--all option)";
		continue;
	fi

	if [[ "test-xml-response-deserialize-callback.sh" == "$t" && -z "$xml" ]]; then
		echo "need xml support in postgres for this callback (--xml or -a|--all option)";
		continue;
	fi

	$test_dir/$t
done
