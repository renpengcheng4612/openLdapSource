docker run -itd -p 8080:80 -p 389:389  --name openshift-ldap  openshift/openldap-2441-centos7

docker run -itd --name centos-radius centos

docker exec -it centos-ldap /bin/bash

docker exec -it openshift-ldap /bin/bash

docker run -d --name openldap_code  -p 389:389 -p 636:636    openldapcode-centos

docker run  -itd --name test9  -p 399:389  -p 638:636  test1  /bin/sh

docker exec -it test9  /bin/bash

docker  run  -it  image_id  /bin/sh