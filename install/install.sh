#!/bin/sh
#
# Copyright (C) 2014 Andreas Bank, andreas.mikael.bank@gmail.com
# (Un)installation script for (QATools complement) abused


if [ "$UID" -ne "0" ] ; then
	echo "You must be root to (un)install abused! Prepend 'sudo'."
	exit 1
fi
echo "If you on a later occasion decide to uninstall use: $0 [uninstal]"
echo
case "$1" in
	uninstall)
		echo "Uninstalling..."
		service abused stop
		chkconfig --del abused
		rm /etc/init.d/abused
		rm /usr/local/bin/abused
		cd ..
		make clean
		;;
	*)
		echo "Installing..."
		cd ../
		make
		make install
		make clean
		cd install
		cp abused /etc/init.d
		chkconfig --add abused
		service abused start
esac
exit 0
