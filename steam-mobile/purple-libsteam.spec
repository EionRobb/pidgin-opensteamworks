%global plugin_name libsteam
%global dir_name steam-mobile

%global commit0 0f51fd6219b80aed9c466d1461a076fc2403a1a3
%global shortcommit0 %(c=%{commit0}; echo ${c:0:7})
%global date 20160618

Name: purple-%{plugin_name}
Version: 1.6.1
Release: 12.%{date}git%{shortcommit0}%{?dist}
Summary: Steam plugin for Pidgin/Adium/libpurple

License: GPLv3
URL: https://github.com/EionRobb/pidgin-opensteamworks
Source0: https://github.com/EionRobb/pidgin-opensteamworks/archive/%{commit0}.tar.gz#/pidgin-opensteamworks-%{shortcommit0}.tar.gz

BuildRequires: pkgconfig(glib-2.0)
BuildRequires: pkgconfig(purple)
BuildRequires: pkgconfig(json-glib-1.0)
BuildRequires: pkgconfig(zlib)
BuildRequires: pkgconfig(nss)
BuildRequires: pkgconfig(gnome-keyring-1)
BuildRequires: gcc

%package -n pidgin-%{plugin_name}
Summary: Adds pixmaps, icons and smileys for Steam protocol
BuildArch: noarch
Requires: %{name} = %{version}-%{release}
Requires: pidgin

%description
Adds support for Steam to Pidgin, Adium, Finch and other libpurple 
based messengers.

%description -n pidgin-%{plugin_name}
Adds pixmaps, icons and smileys for Steam protocol implemented by steam-mobile.

%prep
%setup -qn pidgin-opensteamworks-%{commit0}

# fix W: wrong-file-end-of-line-encoding
perl -i -pe 's/\r\n/\n/gs' README.md

%build
cd %{dir_name}
export CFLAGS="%{optflags}"
export LDFLAGS="%{__global_ldflags}"
%make_build

%install
cd %{dir_name}
%make_install
chmod 755 %{buildroot}%{_libdir}/purple-2/%{plugin_name}.so

%files
%{_libdir}/purple-2/%{plugin_name}.so
%doc README.md
%license %{dir_name}/LICENSE

%files -n pidgin-%{plugin_name}
%{_datadir}/pixmaps/pidgin/protocols/*/steam.png

%changelog
* Tue Jun 21 2016 Vitaly Zaitsev <vitaly@easycoding.org> - 1.6.1-12.20160618git0f51fd6
- Updated to latest Git snapshot. Added missing LDFLAGS to %build.

* Sun Jun 19 2016 Vitaly Zaitsev <vitaly@easycoding.org> - 1.6.1-11.20160618gitcd5a294
- Updated to latest Git snapshot.

* Sat Jun 18 2016 Vitaly Zaitsev <vitaly@easycoding.org> - 1.6.1-10.20160416gitbf7dd28
- Updated package description.

* Sun Jun 12 2016 Vitaly Zaitsev <vitaly@easycoding.org> - 1.6.1-9.20160416gitbf7dd28
- Removed empty configure script.

* Mon May 02 2016 Vitaly Zaitsev <vitaly@easycoding.org> - 1.6.1-8.20160416gitbf7dd28
- Updated to latest version from Git.

* Fri Mar 04 2016 Vitaly Zaitsev <vitaly@easycoding.org> - 1.6.1-7.20160218git5a5beba
- Updated to latest version from Git.

* Tue Feb 16 2016 Vitaly Zaitsev <vitaly@easycoding.org> - 1.6.1-6.20160216git9d51f30
- Updated to latest version from Git.

* Tue Jan 12 2016 Vitaly Zaitsev <vitaly@easycoding.org> - 1.6.1-5.20160108git8646d36
- Updated to latest version from Git.

* Thu Dec 24 2015 Vitaly Zaitsev <vitaly@easycoding.org> - 1.6.1-4.20151224gitef6215f
- Updated to latest version.

* Fri Dec 04 2015 Vitaly Zaitsev <vitaly@easycoding.org> - 1.6.1-3.20151204git72fdb9d
- Added license file.

* Sun Nov 29 2015 Vitaly Zaitsev <vitaly@easycoding.org> - 1.6.1-2.20151115git5aef56a
- Applyed Maxim Orlov's fixes.

* Wed Oct 14 2015 Vitaly Zaitsev <vitaly@easycoding.org> - 1.6.1-1
- Created first RPM spec for Fedora/openSUSE.
