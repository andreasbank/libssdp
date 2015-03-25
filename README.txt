COPYRIGHT NOTICE:
  Copyright(C) by Andreas Bank, andreas.mikael.bank@gmail.com

[A]used is  [B]anks [U]ber-[S]exy-[E]dition [D]aemon"

BUILD & INSTALL:

  1. Download both the Makefile and abused.c in one (same) directory
  2. In a terminal run `make´
  3. Then run `chmod 755 abused´
  4. At last run `sudo cp <path-to-download-directory>/abused /usr/local/bin´

BUILD & INSTALL AS A DAEMON:

To install the abused daemon open a terminal and run:

  1. cd <path-to-downloaded-files>/install
  2. sudo ./install.sh

To uninstall the abused daemon:

  1. cd <path-to-downloaded-files>/install
  2. sudo ./install.sh uninstall


What the install process does is:
 -compile the executable
 -copy the executable to /usr/local/bin
 -copy the init.d script (for boot startup)
 -configure for start boot on runlevels 2, 3, 4 and 5

The uninstall process undoes the steps the install process does.

If you encounter an error please contact me on andreas.mikael.bank@gmail.com.
