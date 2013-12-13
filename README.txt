Provision tasks extra
=====================

This code extends Aegir's back-end with some additional commands.

Supported commands are:

- drush cache-clear
- drush cache-clear drush
- drush core-cron
- drush registry-rebuild
- drush registry-rebuild --fire-bazooka (Drupal 7 only)
- drush updatedb

Extra bundled modules:
- HTTP Basic Authentication (http_basic_auth)
- Sync (provision_sync)

INSTALLATION
------------

This code is for provision, so it needs to be uploaded
in the ~/.drush/ directory of your Aegir backend.

You will need hosting_tasks_extra module uploaded
in the modules directory of the hostmaster platform
to be able to use this extension from the frontend:

http://drupal.org/project/hosting_tasks_extra

Requires registry_rebuild Drush extension uploaded
also in the ~/.drush/ directory of your Aegir backend.

http://drupal.org/project/registry_rebuild

