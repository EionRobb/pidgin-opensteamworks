A plugin to connect to Steam Friends/Steam IM for libpurple/Pidgin/Adium.

As of version 1.0 the (new) plugin will connect to Steam *without Steam running* via the Steam mobile API and thus works on Linux and OSX.

Versions less than 1.0 of this plugin use [Open Steamworks](http://opensteamworks.org/) to connect to a running copy of Steam.

Download .dll's and .so's for your system at the [Downloads Page](https://github.com/EionRobb/pidgin-opensteamworks/releases)

Download the Adium plugin from [adiumxtras.com](http://adiumxtras.com/index.php?a=xtras&xtra_id=8339)

How to Install on Windows
=========================
  * Download the latest dll from the [Downloads Page](https://github.com/EionRobb/pidgin-opensteamworks/releases)
  * Copy dll into your `Program Files (x86)\Pidgin\plugins` folder (or `Program Files\Pidgin\plugins` on 32-bit Windows)
  * Extract [the icons](https://github.com/EionRobb/pidgin-opensteamworks/raw/master/steam-mobile/releases/icons.zip) into your `Program Files (x86)\Pidgin\pixmaps\pidgin\protocols` folder (or `Program Files\Pidgin\pixmaps\pidgin\protocols`)

If this is the first time you have downloaded this plugin:
  * Download [the json-glib library](https://github.com/EionRobb/pidgin-opensteamworks/raw/master/steam-mobile/libjson-glib-1.0.dll) into your `Program Files (x86)\Pidgin` folder (or `Program Files\Pidgin`), _**NOT** into the plugins folder_

How to install on Linux
=======================
  * Download the latest .so file from the [Downloads Page](https://github.com/EionRobb/pidgin-opensteamworks/releases)
  * Copy the file to ```~/.purple/plugins```

How to install on Fedora
=====================
On Fedora you can install [package](https://apps.fedoraproject.org/packages/purple-libsteam) from Fedora's main repository:

```
  sudo dnf install purple-libsteam pidgin-libsteam
```

How to install on CentOS/RHEL
=====================
On CentOS/RHEL you can install [package](https://apps.fedoraproject.org/packages/purple-libsteam) from Fedora's [EPEL7](http://fedoraproject.org/wiki/EPEL) repository:

```
  sudo yum install purple-libsteam pidgin-libsteam
```

How to Build RPM package for Fedora/openSUSE/CentOS/RHEL
=====================
  ```
  sudo yum -y install rpm-build gcc json-glib-devel libpurple-devel zlib-devel make automake glib2-devel libgnome-keyring-devel nss-devel spectool
  mkdir -p ~/rpmbuild/{BUILD,BUILDROOT,RPMS,SOURCES,SPECS,SRPMS}
  wget https://github.com/EionRobb/pidgin-opensteamworks/raw/master/steam-mobile/purple-libsteam.spec -O ~/rpmbuild/SPECS/purple-libsteam.spec
  spectool --all --get-files ~/rpmbuild/SPECS/purple-libsteam.spec --directory ~/rpmbuild/SOURCES/
  rpmbuild -ba ~/rpmbuild/SPECS/purple-libsteam.spec
  ```

How to Build on Ubuntu/Debian
=====================
  * Download the [latest tarball](https://github.com/EionRobb/pidgin-opensteamworks/releases) or `git clone` the repository
  * Make sure you have the development packages/headers: `sudo apt-get install libpurple-dev libglib2.0-dev libjson-glib-dev libgnome-keyring-dev libnss3-dev libsecret-1-dev`
  * Build and install the plugin `cd steam-mobile && make && sudo make install`
  * To build a .deb package: `sudo apt install checkinstall && cd steam-mobile && sudo checkinstall --pkgname=pidgin-opensteamworks --arch=amd64 --pkglicense=GPL-3.0 --pkgsource https://github.com/EionRobb/pidgin-opensteamworks --pkgversion=1.7 --requires="libpurple0,libglib2.0-0,libjson-glib-1.0-0,libgnome-keyring0,libnss3,libsecret-1-0"`

How to Build on Linux
=====================
  * Download the [latest tarball](https://github.com/EionRobb/pidgin-opensteamworks/releases) or `git clone` the repository
  * Make sure you have the development packages/headers for libpurple, glib-2.0, libjson-glib, gnome-keyring, nss, libsecret
  * Run `cd steam-mobile && make && sudo make install`

Changelog
========= 
  * v1.7:  Add an option to appear as a 'web' user instead of 'mobile' user
  * v1.7:  Fix to show the name of people who are requesting to add you to their friends list
  * v1.7:  Add the option to redeem game keys to the account menu
  * v1.7:  Fixes for displaying the in-game game name
  * v1.7:  Improvements to steam guard and captcha handling
  * v1.7:  Ignore invites from groups/clans
  * v1.7:  Fixes for handling rate-limiting from the server
  * v1.7:  Fixes a bunch of crashes
  * v1.7:  Add libgcrypt, mbedtls, openssl as optional crypto backends
  * v1.7:  Bitlbee and Adium compatibility improvements
  * v1.7:  Switch to use libsecret instead of gnome-keyring for Telepathy-Haze users
  * v1.6.1 - Fix for repeated offline history
  * v1.6 - Fixes logins and crashes for UTF8 characters in game names, downloads offline history
  * v1.5.1 - Fixes the infinite captcha login loop
  * v1.5 - Adds 'Launch/Join Game' options to buddy menu, fix sending messages with < or > in them, fix for renaming a group in the buddy list, (beta) support for captcha image and two-factor auth, better handling of expired access tokens and steam guard tokens
  * v1.4 - Display Steam nicknames, improvements to sign-in and when servers are down, display buddy status in the buddy list
  * v1.3 - Fixes for Telepathy-Haze (Empathy), adds an option to change your status in Pidgin when in game, improved connection/disconnection handling, fixes for /me messages
  * v1.2 - Fixes logins giving a 404 error, some buddies appearing offline, historical message timestamps being incorrect,  and adds an option to always use HTTPS
  * v1.1 - Fixes friend accept/deny not working, messages being received twice and improves some reconnection code
