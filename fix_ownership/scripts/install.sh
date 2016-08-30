#!/bin/bash

if [ $(id -u) != 0 ]; then
  printf "***********************************************\n"
  printf "* Error: You must run this with sudo or root. *\n"
  printf "***********************************************\n"
  exit 1
fi

DIR=$(dirname "$0")

cp $DIR/fix-ownership.sh /usr/local/bin
chown root:root /usr/local/bin/fix-ownership.sh
chmod u+x /usr/local/bin/fix-ownership.sh
echo "aegir ALL=NOPASSWD: /usr/local/bin/fix-ownership.sh" > /etc/sudoers.d/fix-ownership
chmod 0440 /etc/sudoers.d/fix-ownership
