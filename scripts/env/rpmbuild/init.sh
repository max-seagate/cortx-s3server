#!/bin/sh
#
# Copyright (c) 2020 Seagate Technology LLC and/or its Affiliates
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# For any questions about this software or licensing,
# please email opensource@seagate.com or cortx-questions@seagate.com.
#

set -e


SCRIPT_PATH=$(readlink -f "$0")
BASEDIR=$(dirname "$SCRIPT_PATH")
S3_SRC_DIR="$BASEDIR/../../../"
CURRENT_DIR=`pwd`

#function to install/upgrade cortx-py-utils rpm
install_cortx_py_utils() {
  #rpm -q cortx-py-utils && yum remove cortx-py-utils -y && yum install cortx-py-utils -y
  if rpm -q cortx-py-utils ; then
    yum remove cortx-py-utils -y
  fi
  yum install cortx-py-utils -y

  # install cortx-py-utils prerequisite
  pip3 install -r /opt/seagate/cortx/utils/conf/requirements.txt --ignore-installed
}

# function to install all prerequisite for dev vm 
install_pre_requisites() {

  # install kafka server
  sh ${S3_SRC_DIR}/scripts/kafka/install-kafka.sh -c 1 -i $HOSTNAME
  
  #create topic
  sh ${S3_SRC_DIR}/scripts/kafka/create-topic.sh -c 1 -i $HOSTNAME

  # install or upgrade cortx-py-utils
  install_cortx_py_utils

  # install configobj
  pip3 install configobj
}

usage() {
  echo "Usage: $0
  optional arguments:
       -a    setup s3 rpmbuild autonomously
       -h    show this help message and exit" 1>&2;
  exit 1; }

if [[ $# -eq 0 ]] ; then
  source ${S3_SRC_DIR}/scripts/env/common/setup-yum-repos.sh
  #install pre-requisites on dev vm
  install_pre_requisites
else
  while getopts "ah" x; do
      case "${x}" in
          a)
              yum install createrepo -y
              easy_install pip
              read -p "Git Access Token:" git_access_token
              source ${S3_SRC_DIR}/scripts/env/common/create-cortx-repo.sh -G $git_access_token
              # install configobj
              pip3 install configobj
              ;;
          *)
              usage
              ;;
      esac
  done
  shift $((OPTIND-1))
fi

yum install -y ansible facter rpm-build

cd ${BASEDIR}/../../../ansible

# Update ansible/hosts file with local ip
cp -f ./hosts ./hosts_local
sed -i "s/^xx.xx.xx.xx/127.0.0.1/" ./hosts_local

# Setup rpmbuild env
ansible-playbook -i ./hosts_local --connection local rpm_build_env.yml -v  -k   --extra-vars "s3_src=${S3_SRC_DIR}"

rm -f ./hosts_local

cd ${CURRENT_DIR}
