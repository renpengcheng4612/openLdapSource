Platform and software version:

OS:CentOS5.3

gcc:4.1.2
BerkeleyDB:4.8.24
openldap:2.4.19

1. Prerequisite software

GCC
BerkeleyDB
openssl(installed by default)

2.Q&A

(1)不装GCC，执行./configure时，
出现错误：
configure: error: Unable to locate cc(1) or suitable replacement.  Check PATH or set CC.

解决方法:
install gcc
[root@localhost openldap-2.4.19]#yum -y install gcc

(2)不装BerkeleyDB,执行./configure时，
出现错误：
configure: error: BDB/HDB: BerkeleyDB not available


解决方法:
[root@localhost BerkeleyDB]# unzip db-4.8.24.zip
[root@localhost BerkeleyDB]# cd db-4.8.24
[root@localhost db-4.8.24]# cd build_unix/
[root@localhost db-4.8.24]# ../dist/configure
[root@localhost db-4.8.24]# make
[root@localhost db-4.8.24]# make install


[root@localhost openldap-2.4.19]#export CPPFLAGS="-I/usr/local/BerkeleyDB.4.8/include"
[root@localhost openldap-2.4.19]#export LDFLAGS="-L/usr/local/lib -L/usr/local/BerkeleyDB.4.8/lib

-R/usr/local/BerkeleyDB.4.8/lib"
[root@localhost openldap-2.4.19]#export LD_LIBRARY_PATH="/usr/local/BerkeleyDB.4.6/lib"


3.install openldap



[root@localhost openldap-2.4.19]#./configure
[root@localhost openldap-2.4.19]#make depend
[root@localhost openldap-2.4.19]#make
[root@localhost openldap-2.4.19]#make test
[root@localhost openldap-2.4.19]#make install



# AUTO-GENERATED FILE - DO NOT EDIT!! Use ldapmodify.
# CRC32 07b0201e
dn: olcDatabase={0}config
objectClass: olcDatabaseConfig
olcDatabase: {0}config
olcAccess: {0}to *  by * none
olcAddContentAcl: TRUE
olcLastMod: TRUE
olcMaxDerefDepth: 15
olcReadOnly: FALSE
olcRootDN: cn=config
olcSyncUseSubentry: FALSE
olcMonitoring: FALSE
structuralObjectClass: olcDatabaseConfig
entryUUID: 9fdea814-898f-103d-9d86-9d1a73523e5c
creatorsName: cn=config
createTimestamp: 20230518061900Z
entryCSN: 20230518061900.983953Z#000000#000#000000
modifiersName: cn=config
modifyTimestamp: 20230518061900Z


# 异常记录：
Failed to get D-Bus connection: Operation not permitted


Failed to get D-Bus connection: No such file or directory

systemctl status sshd

docker清理
docker system prune来自动清理空间，参考下面：

该指令默认会清除所有如下资源：
已停止的容器（container）
未被任何容器所使用的卷（volume）
未被任何容器所关联的网络（network）
所有悬空镜像（image）。

