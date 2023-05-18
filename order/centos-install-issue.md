How To Install OpenLDAP Server on CentOS 8? As of January 2021, OpenLDAP Server still has no pre-build binary packages for CentOS 8 systems. So if you want to run OpenLDAP Server on your CentOS computer, you have to built it from source code yourself.

1. Confirm that there is no openldap-servers package for CentOS 8.

(Log in to CentOS 8)
herong$ sudo dnf install openldap-servers

No match for argument: openldap-servers
Error: Unable to find a match: openldap-servers
2. Install required packages to build OpenLDAP Server from source code.

herong$ sudo dnf install wget vim cyrus-sasl-devel \
libtool-ltdl-devel openssl-devel libdb-devel make libtool \
autoconf  tar gcc perl perl-devel

Install  18 Packages
Upgrade  31 Packages
3. Create a "ldap" user to run "ldapd" service.

herong$ sudo useradd -r -M -d /var/lib/openldap -u 55 \
-s /usr/sbin/nologin ldap
4. Download the latest OpenLDAP server source code.

herong$ wget ftp://ftp.openldap.org/pub/OpenLDAP\
/openldap-release/openldap-2.4.9.tgz

...
Length: 4440456 (4.2M) (unauthoritative)
...
(25.0 KB/s) - ‘openldap-2.4.9.tgz’ saved [4440456]
5. Configure the build process. I see 2 problems in the output.

herong$ tar xzf openldap-2.4.9.tgz

herong$ cd openldap-2.4.9

herong$ ./configure --prefix=/usr --sysconfdir=/etc --disable-static \
--enable-debug --with-tls=openssl --with-cyrus-sasl --enable-dynamic \
--enable-crypt --enable-spasswd --enable-slapd --enable-modules \
--enable-rlookups --enable-backends=mod --disable-ndb --disable-sql \
--disable-shell --disable-bdb --disable-hdb --enable-overlays=mod

...
checking for sys/un.h... yes
checking openssl/ssl.h usability... yes
checking openssl/ssl.h presence... yes
checking for openssl/ssl.h... yes
checking for SSL_library_init in -lssl... no
checking for ssl3_accept in -lssl... no
configure: error: Could not locate TLS/SSL package


6. Search Internet for suggestions. I found a comment from Tom at https://openldap.org/lists/openldap-technical/201612/msg00014.html.

So, as a followup to anyone else who may hit this issue,
OpenLDAP 2.4.44 won't build (without a set of patches) using
OpenSSL 1.1.0c. I downloaded the older OpenSSL 1.0.2j and
everything built fine. Hopefully the patches that allow OpenSSL 1.1.0
will be rolled into OpenLDAP 2.4.45 but it may be longer as there seem
to be a number of OpenSSL API changes.
7. Try it again witout "--with-tls=openssl". I see no problems.

herong$ ./configure --prefix=/usr --sysconfdir=/etc --disable-static \
--enable-debug --with-cyrus-sasl --enable-dynamic \
--enable-crypt --enable-spasswd --enable-slapd --enable-modules \
--enable-rlookups --enable-backends=mod --disable-ndb --disable-sql \
--disable-shell --disable-bdb --disable-hdb --enable-overlays=mod

...
config.status: executing default commands
Making servers/slapd/backends.c
Add config ...
Add ldif ...
Making servers/slapd/overlays/statover.c
Please run "make depend" to build dependencies
8. Continue with the build process. I see another error.

herong$ make depend
...
make[2]: Leaving directory '/home/herong/openldap-2.4.9/doc/man'
make[1]: Leaving directory '/home/herong/openldap-2.4.9/doc'

herong$ make
...
cc -g -O2 -I../../include -I../../include -c -o setproctitle.o setproctitle.c
cc -g -O2 -I../../include -I../../include -c -o getpeereid.o getpeereid.c
getpeereid.c: In function ‘lutil_getpeereid’:
getpeereid.c:64:15: error: storage size of ‘peercred’ isn’t known
struct ucred peercred;
^~~~~~~~
make[2]: *** [<builtin>: getpeereid.o] Error 1
make[2]: Leaving directory '/home/herong/openldap-2.4.9/libraries/liblutil'
make[1]: *** [Makefile:295: all-common] Error 1
make[1]: Leaving directory '/home/herong/openldap-2.4.9/libraries'
make: *** [Makefile:311: all-common] Error 1
Ok. Release 2.4.9 seems to be too new. I should try an older version.

