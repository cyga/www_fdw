#!/bin/bash
sucess=0
failed=0
function test
{
	if [ "$1" == "$2" ]; then
		let success=$success+1
		echo "Ok       $3"
	else
		let failed=$failed+1
		echo "Failed   $3"
		echo -e "  Result:   '$1'"
		echo -e "  Expected: '$2'"
	fi
}
trap 'let all=$success+$failed;echo;echo "Tests run: $all";echo "Ok: $success";echo "Failed: $failed"' EXIT
