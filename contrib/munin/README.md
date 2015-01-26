# ebusd-munin
Collection of munin plugins (see http://munin-monitoring.org/) related to ebusd (see http://github.com/john30/ebusd/).

These plugins can easily be used to feed a munin node with data from a local ebusd instance.
Personally, I use them for checking mainly temperatures, pressures and earned energy of my heatpump. Looking at the graphs produced by munin I find it rather easy to know what's going on in the heatpump.

# Installation:
- prerequisite is of course a running munin installation on the local host
- to install the plugins:
  - git checkout https://github.com/john30/ebusd-munin.git
  - sudo cp ebusd-munin/plugins/* /usr/share/munin/plugins/
  - sudo munin-node-configure --shell
  - run all "ln -s" commands printed by munin-node-configure and containing "ebusd_"
  - sudo service munin-node reload

that's it.
