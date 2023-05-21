From  centos:centos7
ENV LANG C.utf8
USER root

# 设置东时区为东8区
ENV TZ=Asia/Shanghai

# 将 openldap 的源码添加到容器中
RUN   mkdir  openldap-code
COPY  /openldap-2.6.4     /openldap-code/openldap-2.6.4
COPY  /openldap-OPENLDAP_REL_ENG_2_3   /openldap-code/openldap-OPENLDAP_REL_ENG_2_3

# 将 jar 包拷贝 添加到容器中
RUN    mkdir          4a-ldif
COPY   *.ldif       ./4a-ldif
COPY   slapd.conf   /etc/openldap/

RUN    mkdir      myschema
COPY   *.schema       /myschema/

# 查看当前目录文件
RUN ls  /

# 暴露 389 636 端口
EXPOSE 389 636
USER root





