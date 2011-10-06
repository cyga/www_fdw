#!/bin/bash
sucess=0
failed=0
function test
{
	if [ "$1" == "$2" ]; then
		let success=$success+1
		echo "Ok"
	else
		let failed=$failed+1
		echo "Failed"
	fi
}
trap 'let all=$success+$failed;echo;echo "Tests run: $all";echo "Ok: $success";echo "Failed: $failed"' EXIT
