#!/usr/bin/env python3
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

import sys
from cortx.utils.process import SimpleProcess
from setupcmd import SetupCmd

services_list = ["haproxy", "s3backgroundconsumer", "s3authserver"]
all_services_instances_list = ["s3backgroundproducer@*", "s3server@*"]

class ResetCmd(SetupCmd):
  """Reset Setup Cmd."""
  name = "reset"

  def __init__(self, config: str):
    """Constructor."""
    try:
      super(ResetCmd, self).__init__(config)
    except Exception as e:
      raise e

  def is_service_active(self, s3service):
    """Return True if service is running."""
    self.service = s3service

    cmd = ['/bin/systemctl', 'status',  f'{self.service}']
    handler = SimpleProcess(cmd)
    res_op, res_err, res_rc = handler.run()
    if res_rc != 0:
      raise Exception(f"{cmd} failed with err: {res_err}, out: {res_op}, ret: {res_rc}")

    res_op_list = res_op.split('\n')
    for line in res_op_list:
        if 'Active:' in line:
           if '(running)' in line:
               return True
    return False

  def shutdown_active_service(self):
    """Stop service and return returncode."""
    cmd = ['/bin/systemctl', 'stop',  f'{self.service}']
    handler = SimpleProcess(cmd)
    sys.stdout.write(f"shutting down {self.service}\n")
    res_op, res_err, res_rc = handler.run()
    if res_rc != 0:
      raise Exception(f"{cmd} failed with err: {res_err}, out: {res_op}, ret: {res_rc}")
    else:
      return res_rc

  def shutdown_service(self, s3service):
    """Stop service and return returncode."""
    cmd = ['/bin/systemctl', 'stop',  f'{s3service}']
    handler = SimpleProcess(cmd)
    sys.stdout.write(f"shutting down {s3service}\n")
    res_op, res_err, res_rc = handler.run()
    if res_rc != 0:
      raise Exception(f"{cmd} failed with err: {res_err}, out: {res_op}, ret: {res_rc}")
    else:
      return res_rc

  def shutdown_s3services(self):
   """Shutdown s3 services"""
   for s3services in services_list:
    if self.is_service_active(s3services):
      self.shutdown_active_service()

   for server_instances in all_services_instances_list:
     self.shutdown_service(server_instances)


  def process(self):
    """Main processing function."""
    retval = 0
    sys.stdout.write(f"Processing {self.name} {self.url}\n")
    sys.stdout.write("Shutting down s3 services...\n")
    self.shutdown_s3services()
    return retval
