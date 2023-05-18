docker run -itd -p 8080:80 -p 389:389  --name openshift-ldap  openshift/openldap-2441-centos7

docker run -itd --name centos-radius centos

docker exec -it centos-ldap /bin/bash

docker exec -it openshift-ldap /bin/bash

docker run -d --name openldap_code  -p 389:389 -p 636:636    openldapcode-centos

docker run  -itd --name openldap_code  -p 399:389  -p 638:636  openldapcode-centos  /bin/sh

docker run  -itd --name openldap_code  -p 389:389  -p 636:636  openldapcode-centos  /bin/sh

docker run  -itd --name  test1   -p 389:389  -p 636:636  openldap_code_centos8_with_yum_repo  /bin/sh



docker run  -itd --name  test2  --privileged=true  -p 389:389  -p 636:636  openldap-centos-7 /sbin/init

docker run  -itd --name  test2  --privileged=true  -e "container=docker"  -v /sys/fs/cgroup:/sys/fs/cgroup -p 389:389  -p 636:636  openldap-centos-7    /usr/sbin/init

docker exec -it test2  /bin/bash


docker exec -it openldap_code /bin/bash


docker  run  -it  image_id  /bin/sh



