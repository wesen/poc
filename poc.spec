Name:           poc
Version:        0.4.1
Release:        1
Summary:        POC mp3 streamer

Group:          Development/Audio
License:        BSD
URL:            http://www.bl0rg.net/software/poc
Source0:        http:/www.bl0rg.net/software/poc/poc-0.4.1.tar.gz
BuildRoot:      %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

BuildRequires:  gcc
#Requires:       
#Requires(pre):  
#Requires(post): 
#Conflicts:      
#Obsoletes:      
#BuildConflicts: 

%description
Long description of POC

%prep
%setup -q


%build
make %{?_smp_mflags}


%install
rm -rf $RPM_BUILD_ROOT
make install DESTDIR=$RPM_BUILD_ROOT PREFIX=/usr
# Note: the find_lang macro requires gettext

%check || :
#make test
#make check


%clean
rm -rf $RPM_BUILD_ROOT


# ldconfig's for packages that install %{_libdir}/*.so.*
# -> Don't forget Requires(post) and Requires(postun): /sbin/ldconfig
# ...and install-info's for ones that install %{_infodir}/*.info*
# -> Don't forget Requires(post) and Requires(preun): /sbin/install-info

%post
# /sbin/ldconfig
# /sbin/install-info %{_infodir}/%{name}.info %{_infodir}/dir 2>/dev/null || :

%files
%defattr(-,root,root,-)
%doc README 
%{_bindir}/*
%{_mandir}/man[^3]/*

#%files devel
#%defattr(-,root,root,-)
#%doc HACKING
#%{_libdir}/*.a
#%{_libdir}/*.so
#%{_mandir}/man3/*


%changelog
* Sat Feb 26 2005 Manuel Odendahl <manuel@bl0rg.net> - 1109434658:0.4.1
- Initial RPM release.
