#!/bin/bash
set -e

a_flag='false'
s_flag='false'
c_flag='false'
t_flag='false'
u_flag='false'
h_flag='false'
help_str="supposed: a(all)s(server)c(client)t(thired_party)u(utils)h(share) such as ./build.sh -sc"

if [ $# -eq 0 ]; then
	echo $help_str
	exit 1
fi

while getopts 'asctuh' flag; do
	case "${flag}" in
		a) a_flag='true' ;;
		s) s_flag='true' ;;
		c) c_flag='true' ;;
		t) t_flag='true' ;;
		u) u_flag='true' ;;
        h) h_flag='true' ;;
		*) error="Unexpected option ${flag}, "$help_str ;;
	esac
done

echo "set build all as "$a_flag
echo "set build server as "$s_flag
echo "set build client as "$c_flag
echo "set build third_party as "$t_flag
echo "set build utils as "$u_flag
echo "set build share as "$h_flag

BuildThirdParty()
{
	echo "Build third_party"
	pushd third_party > /dev/null
	./third_party_build_all.sh
	popd > /dev/null
}
BuildUtils()
{
    echo "Build utils"
    pushd utils > /dev/null
    ./utils_build.sh
    popd > /dev/null
}
BuildShare()
{
    echo "Build proto"
    pushd SCShare > /dev/null
    ./scshare_build.sh
    popd > /dev/null
}
BuildServer()
{
    echo "Build server"
    pushd server > /dev/null
    ./server_build.sh
    popd > /dev/null
}
BuildClient()
{
    echo "Build client"
    pushd client > /dev/null
    ./client_build.sh
    popd > /dev/null
}

if [ "$a_flag" == 'true' ]; then
	BuildThirdParty
	BuildUtils
    BuildShare
	BuildServer
    BuildClient
    exit 0;
fi

if [ "$t_flag" == 'true' ]; then
	BuildThirdParty
fi

if [ "$u_flag" == 'true' ]; then
    BuildUtils
fi

if [ "$h_flag" == 'true' ]; then
    BuildShare
fi

if [ "$s_flag" == 'true' ]; then
	BuildServer
fi

if [ "$c_flag" == 'true' ]; then
    BuildClient
fi

if [ ! -z "$error" ]; then
    echo $error
    exit 1
fi
