# 1.0.0-beta.1 (2015-02-XX)

## Breaking Changes
This is the first preview version to ebusd 1.0 which was completely reworked since October 2014.

## Bug fixes
A lot of problems were fixed, such as memory leaks and segmentation faults.

## Features
The daemon is now fully aware of the eBUS protocol and future versions will also support addressing a running daemon as slave.
The main new feature is a completely rewritten configuration file engine, which allows the use of templates and defaults and thus simplifies the configuration files a lot.
On the client side, a lot of commands have been added, such as "listen" (automatically send changed values listened to or polled) and "find".

## Changed files
https://github.com/john30/ebusd/commits/master

Thanks to Roland Jax and other authors for all their work in previous versions!
