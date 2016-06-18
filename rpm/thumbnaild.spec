Name:       thumbnaild
Summary:    Thumbnail generation daemon
Version:    0.0
Release:    1
Group:      Applications/System
License:    LGPLv2.1
URL:        https://github.com/nemomobile/thumbnaild
Source0:    %{name}-%{version}.tar.bz2
BuildRequires:  pkgconfig(Qt5Core)
BuildRequires:  pkgconfig(Qt5DBus)
BuildRequires:  pkgconfig(qt5-boostable)
BuildRequires:  pkgconfig(nemothumbnailer-qt5)

%{!?qtc_qmake5:%define qtc_qmake5 %qmake5}
%{!?qtc_make:%define qtc_make make}

%description
Daemon for generating thumbnail images.

%prep
%setup -q -n %{name}-%{version}

%build
%qtc_qmake5
%qtc_make %{?_smp_mflags}

%install
rm -rf %{buildroot}
%qmake5_install

%files
%defattr(-,root,root,-)
%{_bindir}/thumbnaild
%{_datadir}/dbus-1/services/org.nemomobile.Thumbnailer.service
%{_datadir}/dbus-1/interfaces/org.nemomobile.Thumbnailer.xml

