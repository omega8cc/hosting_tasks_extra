#!/bin/bash

##
# This script is designed to be run by itself on a server.
#
# It will download all additional dependencies for you.
#
# It must be run as root.
#
# Usage:
#
#    wget https://raw.githubusercontent.com/omega8cc/hosting_tasks_extra/refs/heads/5.x-dev/fix_permissions/scripts/standalone-install-fix-permissions-ownership.sh
#    sudo bash standalone-install-fix-permissions-ownership.sh
#
##

HOSTING_TASKS_EXTRA_VERSION="refs/heads/5.x-dev"
SUDOERS_D_PATH=/etc/sudoers.d
SCRIPTS_DESTINATION=/usr/local/bin

if [ "$(id -u)" != 0 ]; then
  printf "***********************************************\n"
  printf "* Error: You must run this with sudo or root. *\n"
  printf "***********************************************\n"
  exit 1
fi

# BOA installs these scripts itself, and its sudoers.d file for each one
# carries a grant for every account: the loop below would replace that file
# with the aegir line alone.
if [[ -e "/root/.barracuda.cnf" ]] || [[ -d "/var/xdrago" ]]; then
  printf "Error: this is a BOA server; BOA installs and maintains these scripts.\n"
  exit 1
fi

TYPES=(ownership permissions)

for TYPE in "${TYPES[@]}"; do
    SCRIPTS=("fix-drupal-platform-${TYPE}" "fix-drupal-site-${TYPE}")
    for SCRIPT in "${SCRIPTS[@]}"; do
      echo "Installing $SCRIPT to ${SCRIPTS_DESTINATION}/${SCRIPT}.sh..."
      # A failed or partial download must never replace the installed
      # script: an empty one would make every task report success while
      # fixing nothing.
      _new="${SCRIPTS_DESTINATION}/${SCRIPT}.sh.new.$$"
      if ! wget -q -O "${_new}" "https://raw.githubusercontent.com/omega8cc/hosting_tasks_extra/${HOSTING_TASKS_EXTRA_VERSION}/fix_${TYPE}/scripts/${SCRIPT}.sh" \
        || [[ "$(head -n 1 "${_new}" 2> /dev/null)" != "#!/bin/bash" ]] \
        || ! grep -q "^echo \"Done setting proper " "${_new}"; then
        rm -f "${_new}"
        printf "Error: the download of %s failed; nothing was changed for it.\n" "${SCRIPT}.sh"
        exit 1
      fi
      mv -f "${_new}" "${SCRIPTS_DESTINATION}/${SCRIPT}.sh"
      chown root:root "${SCRIPTS_DESTINATION}/${SCRIPT}.sh"
      chmod u+x "${SCRIPTS_DESTINATION}/${SCRIPT}.sh"

      echo "Adding sudoers config to ${SUDOERS_D_PATH}/${SCRIPT}"
      echo "aegir ALL=NOPASSWD: ${SCRIPTS_DESTINATION}/${SCRIPT}.sh" > "${SUDOERS_D_PATH}/${SCRIPT}"
      chmod 0440 "${SUDOERS_D_PATH}/${SCRIPT}"
    done
done
