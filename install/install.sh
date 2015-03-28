#!/bin/sh
#
# Copyright (C) 2015 Andreas Bank, andreas.mikael.bank@gmail.com
# (Un)installation script for abused

SERVICE_NAME="abused"
FILE_DIR="install/"

if [ "$(id -u)" -ne "0" ] ; then
	echo "You must be root to (un)install $SERVICE_NAME! Prepend 'sudo'."
	exit 1
fi

case "$1" in
	uninstall)
		echo "Uninstalling..."

		service abused stop

		rm /etc/init.d/$SERVICE_NAME
		rm /usr/local/bin/$SERVICE_NAME

		if [ ! -z "$(which chkconfig)" ]; then
      chkconfig --del $SERVICE_NAME
    elif [ ! -z "$(which update-rc.d)" ]; then
      update-rc.d $SERVICE_NAME remove
    else
      echo "Missing chkconfig or update-rc.d, can not install"
      exit 1
    fi

		cd ..
		;;
	*)
    echo "Building $SERVICE_NAME"
		cd ../
		make
		make install
		make clean

    if [ ! -z "$FILE_DIR" ]; then
		  cd $FILE_DIR
    fi
    cp "$SERVICE_NAME" "$SERVICE_NAME"_new

    echo "The args that will be used for the $SERVICE_NAME daemon are:"
    echo "-uxq -a 127.0.0.1:80 -d"
    while true; do
      read -p "Press 'y' to continue or 'n' to enter new args:" yn
      case $yn in
        [Nn])
          echo "Enter new args for the $SERVICE_NAME daemon:"
          read args
          if [ -f "$SERVICE_NAME"_new ]; then
            sed "s/^args=.*$/args=\" $args\"/" "$SERVICE_NAME"_new > "sed.tmp" && mv "sed.tmp" "$SERVICE_NAME"_new
            break
          else
            echo "Error: cannot read the file $SERVICE_NAME"_new
            exit 1
          fi
          ;;
        [Yy])
          break
          ;;
        *)
          echo "Wrong choise!"
          ;;
      esac
    done
    

		echo "Installing..."
    echo "If you on a later occasion decide to uninstall use: $0 [uninstal]"
    echo

		cp "$SERVICE_NAME"_new /etc/init.d/"$SERVICE_NAME"
    chmod +x "/etc/init.d/$SERVICE_NAME"
    rm "$SERVICE_NAME"_new

		if [ ! -z "$(which chkconfig)" ]; then
		  chkconfig --add $SERVIE_NAME
    elif [ ! -z "$(which update-rc.d)" ]; then
      update-rc.d $SERVICE_NAME defaults
    else
      echo "Missing chkconfig or update-rc.d, can not install"
      exit 1
    fi

		service $SERVICE_NAME start
esac
exit 0

