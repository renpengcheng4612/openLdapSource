Error: Failed to download metadata for repo 'appstream': Cannot prepare internal mirrorlist: No URLs in mirrorlist

cd /etc/yum.repos.d/

# 第一步执行
sed -i 's/mirrorlist/#mirrorlist/g' /etc/yum.repos.d/CentOS-*
# 第二步执行
sed -i 's|#baseurl=http://mirror.centos.org|baseurl=http://vault.centos.org|g' /etc/yum.repos.d/CentOS-*
wget -O /etc/yum.repos.d/CentOS-Base.repo https://mirrors.aliyun.com/repo/Centos-vault-8.5.2111.repo
# 清除
yum clean all
# 缓存
yum makecache
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