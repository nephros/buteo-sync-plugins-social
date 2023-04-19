Name:       buteo-sync-plugins-social
Summary:    Sync plugins for social services
Version:    0.4.0
Release:    1
License:    LGPLv2
URL:        https://git.sailfishos.org/mer-core/buteo-sync-plugins-social
Source0:    %{name}-%{version}.tar.bz2
BuildRequires:  pkgconfig(Qt5Core)
BuildRequires:  pkgconfig(Qt5DBus)
BuildRequires:  pkgconfig(Qt5Sql)
BuildRequires:  pkgconfig(Qt5Network)
BuildRequires:  pkgconfig(Qt5Gui)
BuildRequires:  pkgconfig(Qt5Contacts)
BuildRequires:  qt5-qttools-linguist
BuildRequires:  pkgconfig(mlite5)
BuildRequires:  pkgconfig(buteosyncfw5) >= 0.10.0
BuildRequires:  pkgconfig(libsignon-qt5)
BuildRequires:  pkgconfig(accounts-qt5) >= 1.13
BuildRequires:  pkgconfig(socialcache) >= 0.0.48
BuildRequires:  pkgconfig(libsailfishkeyprovider)
BuildRequires:  pkgconfig(qtcontacts-sqlite-qt5-extensions) >= 0.3.0
BuildRequires:  pkgconfig(libmkcal-qt5) >= 0.5.45
BuildRequires:  pkgconfig(KF5CalendarCore) >= 5.79
BuildRequires:  nemo-qml-plugin-notifications-qt5-devel
Requires: buteo-syncfw-qt5-msyncd
Requires: systemd
Requires(pre):  sailfish-setup
Requires(post): systemd
Obsoletes: sociald < 0.4.0
Provides: sociald

%description
A Buteo plugin which provides data synchronization with various social services.

%files
%defattr(-,root,root,-)
%{_libdir}/buteo-plugins-qt5/oopp/libsociald-client.so
%config %{_sysconfdir}/buteo/profiles/client/sociald.xml
%config %{_sysconfdir}/buteo/profiles/sync/sociald.All.xml
%{_libdir}/libsyncpluginscommon.so.*
%exclude %{_libdir}/libsyncpluginscommon.so
%license COPYING

%package github
Summary:    Provides synchronisation with GitHub
Requires: %{name} = %{version}-%{release}

%description github
%{summary}.

%files github
# notifications:
%{_libdir}/buteo-plugins-qt5/oopp/libgithub-notifications-client.so
%config %{_sysconfdir}/buteo/profiles/client/github-notifications.xml
%config %{_sysconfdir}/buteo/profiles/sync/github.Notifications.xml


%pre github
# notifications
USERS=$(getent group users | cut -d ":" -f 4 | tr "," "\n")
for user in $USERS; do
    USERHOME=$(getent passwd ${user} | cut -d ":" -f 6)
    rm -f ${USERHOME}/.cache/msyncd/sync/client/github-nofitications.xml || :
    rm -f ${USERHOME}/.cache/msyncd/sync/github.Notifications.xml || :
done

%post github
systemctl-user try-restart msyncd.service || :

%prep
%setup -q -n %{name}-%{version}

%build
%qmake5 \
    "CONFIG+=github"
make %{_smp_mflags}

%pre
USERS=$(getent group users | cut -d ":" -f 4 | tr "," "\n")
for user in $USERS; do
    USERHOME=$(getent passwd ${user} | cut -d ":" -f 6)
    rm -f ${USERHOME}/.cache/msyncd/sync/client/sociald.xml || :
    rm -f ${USERHOME}/.cache/msyncd/sync/sociald.github.Notifications.xml || :
done

%install
rm -rf %{buildroot}
%qmake5_install

%post
/sbin/ldconfig || :
systemctl-user try-restart msyncd.service || :

%postun
/sbin/ldconfig || :
