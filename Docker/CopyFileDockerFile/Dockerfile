FROM quay.sz.chinaclear.cn/4a-public/openldap:latest
MAINTAINER BPM-PLATFORM

ENV LANG C.utf8
USER root

# 设置东时区为东8区
ENV TZ=Asia/Shanghai

# 将 jar 包拷贝 添加到容器中
RUN   mkdir       4a-ldif
COPY  *.ldif       ./4a-ldif
COPY  slapd.conf   /etc/openldap/

RUN    mkdir      myschema
COPY   *.schema       /myschema
COPY   run-openldap.sh     /user/local/bin

RUN ls
RUN chmod  +x  ./user/local/bin/*

# 查看当前目录文件
RUN ls  /

# 暴露 389 636 端口
EXPOSE 389 636
USER root

