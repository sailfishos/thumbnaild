Name:       thumbnaild
Summary:    Thumbnail generation daemon
Version:    0.0
Release:    1
License:    BSD or GPLv2+
URL:        https://git.sailfishos.org/mer-core/thumbnaild
Source0:    %{name}-%{version}.tar.bz2
Source1:    %{name}.privileges
BuildRequires:  pkgconfig(Qt5Core)
BuildRequires:  pkgconfig(Qt5DBus)
BuildRequires:  pkgconfig(Qt5Gui)
BuildRequires:  pkgconfig(qt5-boostable)
BuildRequires:  pkgconfig(nemothumbnailer-qt5) >= 0.2.0
BuildRequires:  pkgconfig(libavcodec)
BuildRequires:  pkgconfig(libavformat) >= 11.3
BuildRequires:  pkgconfig(libavutil)
BuildRequires:  pkgconfig(libswscale)
BuildRequires:  pkgconfig(poppler-qt5)
BuildRequires:  pkgconfig(systemd)

%description
Daemon for generating thumbnail images.

%prep
%setup -q -n %{name}-%{version}

%build
%qmake5
make %{?_smp_mflags}

%install
rm -rf %{buildroot}
%qmake5_install

mkdir -p %{buildroot}%{_datadir}/mapplauncherd/privileges.d
install -m 644 -p %{SOURCE1} %{buildroot}%{_datadir}/mapplauncherd/privileges.d/

%files
%defattr(-,root,root,-)
%{_bindir}/thumbnaild
%{_bindir}/thumbnaild-video
%{_bindir}/thumbnaild-pdf
%{_datadir}/mapplauncherd/privileges.d/*
%{_userunitdir}/dbus-org.nemomobile.thumbnaild.service
%{_datadir}/dbus-1/services/org.nemomobile.Thumbnailer.service
%{_datadir}/dbus-1/interfaces/org.nemomobile.Thumbnailer.xml
