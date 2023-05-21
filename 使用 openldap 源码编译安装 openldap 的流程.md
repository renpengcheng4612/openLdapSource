ftp 各版本的代码下载地址：

ftp://ftp.openldap.org/pub/OpenLDAP/openldap-release/


2.3 版本需要的基础环境：

1. 基础系统(库和工具):

- 标准C编译器(必选)       gcc 版本太高 configure 执行文件会出各种问题
- Cyrus SASL 2.1.18+(推荐)
- OpenSSL 0.9.7+(推荐)
- POSIX REGEX软件(必需)


2. SLAPD:

- 提前准备好编译环境： 
  yum install *ltdl* gcc gcc-c++ -y

- BDB和HDB后端需要Oracle Berkeley DB 4.2, 4.4，4.5。强烈建议应用补丁 从Oracle获得一个给定的版本。
- berkeley-db-5.1.29 (OpenLDAP当前与6.x版本不兼容，READEME中明确写出兼容4.4~4.8或5.0~5.1)：
  http://download.oracle.com/berkeley-db/db-5.1.29.tar.gz
  wget http://download.oracle.com/berkeley-db/db-4.8.30.tar.gz

源码安装前先安装所有的依赖包： yum install libtool-ltdl libtool-ltdl-devel gcc openssl openssl-devel -y

2.6 版本需要的基础环境： 
yum -y install vim net-tools telnet gcc libtool libtool-ltdl libtool-ltdl-devel openssl openssl-devel
openssl-libs gnutls gnutls-utils guntls-devel tcp-wrappers-devel tcp-wrappers-libs libdb-devel unixODBC-devel
mysql-devel cyrus-sasl cyrus-sasl-devel autogen-libopts perl-LDAP authconfig nss-pam-ldapd openslp-devel

yum -y install tcp_wrappers*
yum -y install openssh-ldap

- secret ssha :
- {SSHA}jdUNQecQjWYLs/oArKQf438cenZDxIXF
- {SSHA}gEfQ6xel4smSm/GbhKiEdGSitvVnm1LQ
- {SSHA}DKM0nAHI8brJKdIqQWhkI34KX5EwYIpS
- {SSHA}dkxDAqZErGVrooFS3L9DtsWahRt8iDQL

- 代码编译后需要增加的 schema

include /usr/local/openldap-2.6.4/etc/openldap/schema/core.schema include
/usr/local/openldap-2.6.4/etc/openldap/schema/collective.schema include
/usr/local/openldap-2.6.4/etc/openldap/schema/corba.schema include
/usr/local/openldap-2.6.4/etc/openldap/schema/cosine.schema include
/usr/local/openldap-2.6.4/etc/openldap/schema/duaconf.schema include
/usr/local/openldap-2.6.4/etc/openldap/schema/dyngroup.schema include
/usr/local/openldap-2.6.4/etc/openldap/schema/inetorgperson.schema include
/usr/local/openldap-2.6.4/etc/openldap/schema/java.schema include
/usr/local/openldap-2.6.4/etc/openldap/schema/misc.schema include
/usr/local/openldap-2.6.4/etc/openldap/schema/nis.schema include
/usr/local/openldap-2.6.4/etc/openldap/schema/openldap.schema include
/usr/local/openldap-2.6.4/etc/openldap/schema/pmi.schema include
/usr/local/openldap-2.6.4/etc/openldap/schema/ppolicy.schema

- 新增日志文件级别与路径，需要在编译时--enable-debug，否则日志文件输出，不影响调试模式;

loglevel 256 logfile /usr/local/openldap-2.6.4/var/slapd.log

- 这里使用mdb做后端数据库，也可修改为"bdb"参数，在OpenLDAP 官方文档" 11.4. LMDB"章节中有介绍mdb是推荐使用的后端数据库;

database mdb

- 使用mdb做后端数据库时，根据官方文档中说明需要设置一个空间值，" In addition to the usual parameters that a minimal configuration requires, the mdb backend requires a maximum size to be set. This should be the largest that the database is ever anticipated to grow (in bytes). The filesystem must also provide enough free space to accommodate this size."；如果使用bdb做后端数据库，需要将此项参数注释;

maxsize 1073741824

- 修改域名及管理员账户名;

suffix        "dc=chinaclear,dc=cn"
rootdn        "cn=root,dc=chinaclear,dc=cn"

- 使用密文密码，即前面使用slappasswd生成的密文;

rootpw {SSHA}K9+WK/t1e0V0K6pUMOyTsaTwkDBNEDiP

- openldap数据目录，采用mdb时，在相应目录生成" data.mdb"与" lock.mdb"文件；采用bdb时，在相应目录生成" dn2id.bdb"与" id2entry.bdb"，及多个" __db.00*"文件。

directory /usr/local/openldap-2.6.4/var/openldap-data

index objectClass eq

directory /usr/local/openldap-2.6.4/var/openldap-data find / -name openldap-data -type d
