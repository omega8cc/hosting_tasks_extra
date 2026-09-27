Aegir HTTP basic authentication
===============================

Introduction
------------

This is a simple module and drush script for Aegir that allows you to specify a
username and password for HTTP basic authentication per site in Aegir.

If you don't know what Aegir is, you'll probably want to start there and come
back when you really know that you want to use this code.
http://aegirproject.org/

Installation
------------

There are two parts to the code:
- A Drupal module for hostmaster - packaged with hosting_tasks_extra. Install
  this like any other Drupal module into you hostmaster site.
- A provision Drush script - packaged with provision_tasks_extra.
- Aegir sometimes struggles to set the correct permissions on some directories
  that might stop your site from working. Make sure that
  `/var/aegir/config/server_NAME/apache` is executable by 'others', which just
  means that the directory is browsable

Now just enable the module in the Aegir frontend, and you're ready to go.


Usage
-----

When creating or editing a site, you can optionally add a username, password and
message for the HTTP basic authentication. Leaving this blank will do nothing,
but if they are filled in then those credentials will be sent to the backend and
required to access the site.

The password file
-----------------

Each site's file lives in `passwords.d/<site>` beside the web server's
`vhost.d`. It holds a SHA-512 crypt hash with a random salt, and it is
rewritten whenever the site's virtual host is: on Verify, and on install,
migrate, clone, restore, enable and disable. Turning the authentication off,
deleting the site or renaming it removes the old file (on a pack or cluster web
server each member keeps its copy).

When `passwords.d` is setgid in the web server's own group (www-data,
apache, nginx), as BOA keeps it, closed to other accounts, the file takes that
group from the directory and is mode 0640. Anywhere else it stays 0644,
readable by everyone, as it always was, and the task says so in a notice. An
existing `passwords.d` keeps its mode. Files written by earlier versions keep
working until the site's next Verify rewrites them.

Caveats
-------

This module is mostly a demonstration, so it may not work.
Also, the passwords you enter are stored in plain text in the Aegir database
and in the site's Drush alias, and a task run with --debug puts them on the
command line of the master server, so you should not use any secure passwords
for this. Also, HTTP Basic
authentication is sent over the internet as plain text, so you really shouldn't
be using passwords you want to keep secret.
This module is just to stop most people from accessing a site.

History
-------

This module was originally hosted on https://github.com/computerminds/aegir_http_basic (before 6.x-2.x)
