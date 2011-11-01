#!/bin/bash
test_dir=`echo $0 | perl -pe's#(.*/).*#$1#;'`
for t in \
	test-default-json.sh \
	test-default-xml.sh \
	test-request-serialize-callback.sh \
	test-other-response-deserialize-callback.sh \
	test-response-iterate-callback.sh
# need json module for this callback:
#	test-json-response-deserialize-callback.sh \
# need --libxml support in postgres for this callback:
#	test-xml-response-deserialize-callback.sh \
do
	$test_dir/$t
done
