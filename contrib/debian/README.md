
Debian
====================
This directory contains files used to package rocod/roco-qt
for Debian-based Linux systems. If you compile rocod/roco-qt yourself, there are some useful files here.

## roco: URI support ##


roco-qt.desktop  (Gnome / Open Desktop)
To install:

	sudo desktop-file-install roco-qt.desktop
	sudo update-desktop-database

If you build yourself, you will either need to modify the paths in
the .desktop file or copy or symlink your rocoqt binary to `/usr/bin`
and the `../../share/pixmaps/roco128.png` to `/usr/share/pixmaps`

roco-qt.protocol (KDE)
