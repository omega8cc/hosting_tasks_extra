Hosting tasks extra
=====================

This module extends Aegit front-end with some additional tasks.

Supported tasks/commands are:

- Flush all caches (drush cache-clear -d)
- Rebuild registry (drush registry-rebuild -d)
- Run cron (drush core-cron -d)


INSTALL
=================
This code is for hostmaster it needs to be uploaded
in the modules directory of the hostmaster platform.

It requires provision_tasks_extra exstension uploaded
in the ~/.drush/ directory of your Aegir backend.
