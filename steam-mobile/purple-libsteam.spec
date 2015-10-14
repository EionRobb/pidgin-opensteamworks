%define debug_package %{nil}
%define plugin_name libsteam
%define project_name pidgin-opensteamworks
%define purplelib_name purple-%{plugin_name}

Name: %{project_name}
Version: 1.6.1
Release: 1
Summary: Steam plugin for Pidgin/Adium/libpurple
Group: Applications/Productivity
License: GPLv3
URL: https://github.com/EionRobb/pidgin-opensteamworks
Source0: %{project_name}-%{version}.tar.gz
Requires: pidgin-%{plugin_name}

%description
Meta package.

%package -n %{purplelib_name}
Summary: Adds support for Steam to Pidgin
BuildRequires: glib2-devel
BuildRequires: libpurple-devel
BuildRequires: json-glib-devel
BuildRequires: libgnome-keyring-devel
BuildRequires: nss-devel
BuildRequires: gcc
Requires: libpurple
Requires: json-glib
Requires: nss

%package -n pidgin-%{plugin_name}
Summary: Adds pixmaps, icons and smileys for Steam protocol.
Requires: %{purplelib_name}
Requires: pidgin

%description -n %{purplelib_name}
Adds support for Steam to Pidgin, Adium, Finch and other libpurple 
based messengers.

%description -n pidgin-%{plugin_name}
Adds pixmaps, icons and smileys for Steam protocol inplemented by steam-mobile.

%prep -n %{purplelib_name}
%setup -c

%build -n %{purplelib_name}
cd %{project_name}-*/steam-mobile/
make

%install
cd %{project_name}-*/steam-mobile/
%make_install

%files -n %{purplelib_name}
%{_libdir}/purple-2/%{plugin_name}.so

%files -n pidgin-%{plugin_name}
%dir %{_datadir}/pixmaps/pidgin
%dir %{_datadir}/pixmaps/pidgin/protocols
%dir %{_datadir}/pixmaps/pidgin/protocols/16
%{_datadir}/pixmaps/pidgin/protocols/16/steam.png
%dir %{_datadir}/pixmaps/pidgin/protocols/22
%{_datadir}/pixmaps/pidgin/protocols/22/steam.png
%dir %{_datadir}/pixmaps/pidgin/protocols/48
%{_datadir}/pixmaps/pidgin/protocols/48/steam.png

%files

%changelog
* Wed Oct 14 2015 V1TSK <vitaly@easycoding.org> - 1.6.1-1
- Created first RPM spec for Fedora/openSUSE.
