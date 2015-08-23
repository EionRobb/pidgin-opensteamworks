A plugin to connect to Steam Friends/Steam IM.

As of version 1.0 the (new) plugin will connect to Steam **without Steam running** via the Steam mobile API and thus works on Linux and OSX.

Versions less than 1.0 of this plugin use [Open Steamworks](http://opensteamworks.org/) to connect to a running copy of Steam.

Download .dll's and .so's for your system at the [Downloads Page](http://code.google.com/p/pidgin-opensteamworks/wiki/Downloads?tm=2)
Download the Adium plugin from [adiumxtras.com](http://adiumxtras.com/index.php?a=xtras&xtra_id=8339)

Changes:
  * v1.5.1 - Fixes the infinite captcha login loop
  * v1.5 - Adds 'Launch/Join Game' options to buddy menu, fix sending messages with < or > in them, fix for renaming a group in the buddy list, (beta) support for captcha image and two-factor auth, better handling of expired access tokens and steam guard tokens
  * v1.4 - Display Steam nicknames, improvements to sign-in and when servers are down, display buddy status in the buddy list
  * v1.3 - Fixes for Telepathy-Haze (Empathy), adds an option to change your status in Pidgin when in game, improved connection/disconnection handling, fixes for /me messages
  * v1.2 - Fixes logins giving a 404 error, some buddies appearing offline, historical message timestamps being incorrect,  and adds an option to always use HTTPS
  * v1.1 - Fixes friend accept/deny not working, messages being received twice and improves some reconnection code