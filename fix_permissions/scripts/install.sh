#!/bin/bash

if [ "$(id -u)" != 0 ]; then
  printf "***********************************************\n"
  printf "* Error: You must run this with sudo or root. *\n"
  printf "***********************************************\n"
  exit 1
fi

# BOA installs these scripts itself, and its sudoers.d file for each one
# carries a grant for every account: the copy below would replace that file
# with the aegir line alone.
if [[ -e "/root/.barracuda.cnf" ]] || [[ -d "/var/xdrago" ]]; then
  printf "Error: this is a BOA server; BOA installs and maintains these scripts.\n"
  exit 1
fi

DIR=$(dirname "$0")
SCRIPTS=(fix-drupal-platform-permissions fix-drupal-site-permissions)

for SCRIPT in "${SCRIPTS[@]}"; do
  cp "${DIR}/${SCRIPT}.sh" /usr/local/bin
  chown root:root "/usr/local/bin/${SCRIPT}.sh"
  chmod u+x "/usr/local/bin/${SCRIPT}.sh"
  echo "aegir ALL=NOPASSWD: /usr/local/bin/${SCRIPT}.sh" > "/etc/sudoers.d/${SCRIPT}"
  chmod 0440 "/etc/sudoers.d/${SCRIPT}"
done
