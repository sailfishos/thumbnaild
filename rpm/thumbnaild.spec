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
BuildRequires:  pkgconfig(libavcodec)
BuildRequires:  pkgconfig(libavformat) >= 11.3
BuildRequires:  pkgconfig(libavutil)
BuildRequires:  pkgconfig(libswscale)
BuildRequires:  pkgconfig(poppler-qt5)
BuildRequires:  oneshot
%{_oneshot_requires_post}

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
chmod +x %{buildroot}/%{_oneshotdir}/*

%files
%defattr(-,root,root,-)
%{_bindir}/thumbnaild
%{_bindir}/thumbnaild-video
%{_bindir}/thumbnaild-pdf
%{_datadir}/dbus-1/services/org.nemomobile.Thumbnailer.service
%{_datadir}/dbus-1/interfaces/org.nemomobile.Thumbnailer.xml
%{_oneshotdir}/remove-obsolete-tumbler-cache-dir

%post
%{_bindir}/add-oneshot --now remove-obsolete-tumbler-cache-dir

