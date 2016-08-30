#!/bin/bash

if [ $(id -u) != 0 ]; then
  printf "***********************************************\n"
  printf "* Error: You must run this with sudo or root. *\n"
  printf "***********************************************\n"
  exit 1
fi

DIR=$(dirname "$0")

cp $DIR/fix-permissions.sh /usr/local/bin
chown root:root /usr/local/bin/fix-permissions.sh
chmod u+x /usr/local/bin/fix-permissions.sh
echo "aegir ALL=NOPASSWD: /usr/local/bin/fix-permissions.sh" > /etc/sudoers.d/fix-permissions
chmod 0440 /etc/sudoers.d/fix-permissions
