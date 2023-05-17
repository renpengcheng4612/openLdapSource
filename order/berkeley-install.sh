
# cd /usr/local/src
# wget http://download.oracle.com/berkeley-db/db-4.6.18.tar.gz
# tar -zxvf db-4.6.18.tar.gz
# cd db-4.6.18
# cd build_unix
# ../dist/configure --enable-cxx
# make
# make install


wget http://dowload.oracle.com/berkeley-db/db-4.6.18.tar.gz
cd   db-4.6.18/build_unix


export CPPFLAGS="-I/usr/local/BerkeleyDB.4.6/include"
export LDFLAGS="-L/usr/local/lib -L/usr/local/BerkeleyDB.4.6/lib   -R/usr/local/BerkeleyDB.4.6/lib"
export LD_LIBRARY_PATH="/usr/local/BerkeleyDB.4.6/lib"

rpm -e  gcc-8.5.0-4.el8_5.x86_64


https://mirrors6.tuna.tsinghua.edu.cn/gnu/gcc/gcc-4.9.1/gcc-4.9.1.tar.gz
http://mirror.lzu.edu.cn/gnu/gcc/gcc-4.8.0/

../configure --prefix=/usr/local/gcc4.8.0 --enable-checking=release --enable-languages=c,c++ --disable-multilib