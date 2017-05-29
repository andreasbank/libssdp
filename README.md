<div style="text-align:center;">
  <img src="http://www.andreasbank.com/libssdp/libssdp.png" width=128 height=128>
  <h1 align="center">libSSDP</h1>
  <div style="text-align:center;">
    libSSDP - SSDP (UPnP) scanner library and tool (Linux &amp; MacOS X)
    <br />
    <a href="https://www.andreasbank.com"><strong>Visit author page &raquo;</strong></a>
  </div>
</div>

<br>

## Table of contents

- [Get up and running](#get-up-and-running)
- [Repository contents](#repository-contents)
- [Bugs and feature requests](#bugs-and-feature-requests)
- [Documentation](#documentation)
- [Creators](#creators)
- [Copyright and license](#copyright-and-license)

## Get up and running

Get up and running in no time:

- Clone the repo: `git clone https://github.com/andreasbank/libssdp.git`
- Enter the directory `cd libssdp`
- Build `make`
- Run `./ssdpscan -U` and get results

## Repository contents

The repository contains the following files:

    libssdp/
    ├── axis_adaptation/
    │   ├── sql/
    │   │   ├── abused.vpp
    │   │   ├── database_UML.png
    │   │   ├── install.php
    │   │   ├── sql.txt
    │   │   └── uninstall.php
    │   ├── www/
    │   │   ├── capability_icons/
    │   │   │   ├── audio.png
    │   │   │   ├── capability_icons.psd
    │   │   │   ├── dpts.png
    │   │   │   ├── ir.png
    │   │   │   ├── light_control.png
    │   │   │   ├── local_storage.png
    │   │   │   ├── mic.png
    │   │   │   ├── nas.png
    │   │   │   ├── pir.png
    │   │   │   ├── ptz.png
    │   │   │   ├── sd_disk.png
    │   │   │   ├── sd_disk_gray.png
    │   │   │   ├── speaker.png
    │   │   │   ├── status_led.png
    │   │   │   ├── stsnas.png
    │   │   │   └── wlan.png
    │   │   ├── AbusedResult.php
    │   │   ├── AxisFtp.php
    │   │   ├── AxisSyslog.php
    │   │   ├── AxisTelnet.php
    │   │   ├── CapabilityManager.php
    │   │   ├── Controller.php
    │   │   ├── SqlConnection.php
    │   │   ├── SshClient.php
    │   │   ├── down_right_arrow.phg
    │   │   ├── gui.css
    │   │   ├── gui.js
    │   │   ├── gui_list.php
    │   │   ├── index.php
    │   │   ├── post.php
    │   │   └── search_button.png
    │   └── README.txt
    ├── include/
    │   ├── common_definitions.h
    │   ├── configuration.h
    │   ├── daemon.h
    │   ├── log.h
    │   ├── net_definitions.h
    │   ├── net_utils.h
    │   ├── socket_helpers.h
    │   ├── ssdp_cache.h
    │   ├── ssdp_cache_display.h
    │   ├── ssdp_cache_output_format.h
    │   ├── ssdp_common.h
    │   ├── ssdp_filter.h
    │   ├── ssdp_listener.h
    │   ├── ssdp_message.h
    │   ├── ssdp_prober.h
    │   ├── ssdp_static_defs.h
    │   └── string_utils.h
    ├── install/
    │   ├── install.sh
    │   ├── README
    │   └── scanssdp
    ├── src/
    │   ├── configuration.c
    │   ├── daemon.c
    │   ├── log.c
    │   ├── main.c
    │   ├── net_utils.c
    │   ├── socket_helpers.c
    │   ├── ssdp_cache.c
    │   ├── ssdp_cache_display.c
    │   ├── ssdp_cache_output_format.c
    │   ├── ssdp_common.c
    │   ├── ssdp_filter.c
    │   ├── ssdp_listener.c
    │   ├── ssdp_message.c
    │   ├── ssdp_prober.c
    │   └── string_utils.c
    ├── .gitignore
    ├── LICENSE
    ├── Makefile
    ├── README.md
    ├── README.txt
    ├── doxyfile.mk
    ├── dummy_device.c
    ├── test.php
    └── udhisapi.xml

## Bugs, feature requests and contributions

Feel free to write an email! I would be glad to help, or get help :).

## Documentation

libSSDP's documentation is embedded in all the source and header files as doxygen comments and also available as html here [documentation](http://andreasbank.github.io/libssdp).

### Build the documentation locally

You can even build the documentation yourself. The prerequisites are `doxygen` and `dot`. Once this is in place simply run `make docs`. This is recommended since the online documentation might be outdated or in the process of being rebuilt.

## Creators

**Andreas Bank**

- <https://github.com/andreasbank>

## Copyright and license

- Code and documentation copyright 2017 by Andreas Bank.
- Code released under the [GPL3 License](https://github.com/andreasbank/libssdp/blob/master/LICENSE) (will change to MIT soon).
- Docs released under [Creative Commons](https://github.com/andreasbank/libssdp/blob/master/DOCS_LICENSE).
