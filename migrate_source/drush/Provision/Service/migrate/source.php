<?php

/**
 * The migrate_source service base class.
 *
 * Its only job is to subscribe the properties: a context option that no service
 * claims is dropped by provision-save, so without this the association would
 * never survive into the site's backend context.
 */
class Provision_Service_migrate_source extends Provision_Service {

  public $service = 'migrate_source';

  /**
   * Add the needed properties to the site context.
   */
  static function subscribe_site($context) {
    $context->setProperty('migrate_source_db');
    $context->setProperty('migrate_source_uri');
  }

}
