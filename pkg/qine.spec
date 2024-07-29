Name: qine
Version: 0.1
Release: 1%{?dist}
Summary: Not a QNX emulator
License: GPL
Group: System/Emulators/PC
Url: https://github.com/rkapl/qine
%define gcc_ver 10
BuildRequires: cmake
BuildRequires: python311 python311-lark
BuildRequires: gcc%{gcc_ver}-c++ libstdc++6-devel-gcc%{gcc_ver}
Source0:        %{name}-%{version}.tar.xz

%description

%prep
%setup -q

%build
%cmake \
        -DCMAKE_CXX_COMPILER=/usr/bin/g++-10 \
        -DPython3_EXECUTABLE=/usr/bin/python3.11
%cmake_build


%install
%cmake_install

mkdir -p "%{buildroot}/usr/share/qine/terminfo"
tar -C "%{buildroot}/usr/share/qine/terminfo" -xf terminfo/terminfo.tar.xz

%files
%license COPYING.txt
/usr/bin/qine
/usr/share/qine

%changelog

