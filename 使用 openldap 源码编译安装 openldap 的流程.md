2.3 版本需要的基础环境：

1. 基础系统(库和工具):
- 标准C编译器(必选)       gcc 版本太高 configure 执行文件会出各种问题
- Cyrus SASL 2.1.18+(推荐)
- OpenSSL 0.9.7+(推荐)
- POSIX REGEX软件(必需)


2. SLAPD:
- BDB和HDB后端需要Oracle Berkeley DB 4.2, 4.4，4.5。强烈建议应用补丁 从Oracle获得一个给定的版本。
wget http://download.oracle.com/berkeley-db/db-4.8.30.tar.gz


源码安装前先安装所有的依赖包：
yum install libtool-ltdl libtool-ltdl-devel gcc openssl openssl-devel -y

2.6 版本需要的基础环境：
yum -y install vim net-tools telnet gcc libtool libtool-ltdl libtool-ltdl-devel openssl openssl-devel openssl-libs gnutls gnutls-utils guntls-devel tcp-wrappers-devel tcp-wrappers-libs libdb-devel unixODBC-devel mysql-devel cyrus-sasl cyrus-sasl-devel autogen-libopts perl-LDAP authconfig nss-pam-ldapd openslp-devel

yum -y install tcp_wrappers*
yum -y install openssh-ldap

- secret ssha :
- {SSHA}jdUNQecQjWYLs/oArKQf438cenZDxIXF
- {SSHA}gEfQ6xel4smSm/GbhKiEdGSitvVnm1LQ
- {SSHA}DKM0nAHI8brJKdIqQWhkI34KX5EwYIpS
- {SSHA}dkxDAqZErGVrooFS3L9DtsWahRt8iDQL



