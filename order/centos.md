Error: Failed to download metadata for repo 'appstream': Cannot prepare internal mirrorlist: No URLs in mirrorlist

大家都知道Centos8于2021年年底停止了服务，大家再在使用yum源安装时候，
出现下面错误“错误：Failed to download metadata for repo 
‘AppStream’: Cannot prepare internal mirrorlist: No URLs in mirrorlist”

1、进入yum的repos目录

cd /etc/yum.repos.d/
复制
2、修改所有的CentOS文件内容

sed -i 's/mirrorlist/#mirrorlist/g' /etc/yum.repos.d/CentOS-*

sed -i 's|#baseurl=http://mirror.centos.org|baseurl=http://vault.centos.org|g' /etc/yum.repos.d/CentOS-*
复制
3、更新yum源为阿里镜像

wget -O /etc/yum.repos.d/CentOS-Base.repo https://mirrors.aliyun.com/repo/Centos-vault-8.5.2111.repo

wget -O /etc/yum.repos.d/CentOS-Base.repo http://mirrors.cloud.tencent.com/repo/centos8_base.repo

yum clean all

yum makecache
复制
4、yum安装测试是否可以yum安装

yum install wget –y


#关闭firewalld、清空iptables以及关闭selinux的设置
   systemctl disable firewalld
   iptables -F
   setenforce 0

#基本依赖包yum安装
   yum -y install iptables-services
   yum -y install vim
   yum -y install gcc*
   yum -y install tcpdump
   yum -y install lrzsz
   yum -y install cmake
   yum -y install bind-utils
   yum -y install zlib-devel bzip2-devel openssl-devel ncurses-devel sqlite-devel readline-devel tk-devel gdbm-devel db4-devel libpcap-devel xz-devel
   yum -y install libffi-devel -y
   yum -y install libxml*
   yum -y install telnet
   yum -y install git
   yum -y install wget
   yum -y install libtool
   yum -y install libcap-devel
   yum -y install ntpdate

#系统时区设置
cp /usr/share/zoneinfo/Asia/Shanghai  /etc/localtime


# 查看 centos 版本：
cat /etc/redhat-release

CentOS Linux release 8.4.2105


ali yum 仓库：

https://mirrors.aliyun.com/repo/Centos-7.repo