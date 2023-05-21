docker run -itd -p 8080:80 -p 389:389  --name openshift-ldap  openshift/openldap-2441-centos7

docker run -itd --name centos-radius centos

docker exec -it centos-ldap /bin/bash

docker exec -it openshift-ldap /bin/bash

docker run -d --name openldap_code  -p 389:389 -p 636:636    openldapcode-centos

docker run  -itd --name openldap_code  -p 399:389  -p 638:636  openldapcode-centos  /bin/sh

docker run  -itd --name openldap_code  -p 389:389  -p 636:636  openldapcode-centos  /bin/sh

docker run  -itd --name  test1   -p 389:389  -p 636:636  openldap_code_centos8_with_yum_repo  /bin/sh

docker run  -itd --name  test3  --privileged=true  -p 389:389  -p 636:636  openldap-centos-7 /bin/sh

docker run  -itd --name  test2  --privileged=true  -e "container=docker"  -v /sys/fs/cgroup:/sys/fs/cgroup -p 389:389  -p 636:636  openldap-centos-7    /usr/sbin/init

docker run  -itd --name  test5  --privileged=true   -p 388:389   -p 638:636   openldap-code-envirment-cento7    /bin/sh

docker run  -itd --name  test6  --privileged=true   -p 388:389   -p 638:636   openldap-2.6.4-centos-testok:1.0.0   /bin/sh

docker run  -itd --name  test8  --privileged=true   -p 386:389   -p 635:636    centos:centos7.9.2009      /bin/sh

docker exec -it test2  /bin/bash

docker exec -it test3  /bin/bash

docker exec -it test5  /bin/bash

docker exec -it test6  /bin/bash

docker exec -it test8  /bin/bash

docker exec -it openldap_code /bin/bash

docker  run  -it  image_id  /bin/sh