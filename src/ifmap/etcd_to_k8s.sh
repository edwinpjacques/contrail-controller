#!/bin/bash -e
#
# Author:  Edwin P. Jacques (ejacques@juniper.net)
# Copyright (c) 2020 Juniper Networks, Inc. All rights reserved.
#

if [[ "$*" =~ -(help|\?|h)( |$) ]] || [[ -z "$1" ]]; then
cat <<-EOF

Purpose: Filter to generate Kubernetes JSON data from ETCD JSON data.
         JSON is emitted to stdout.

Example conversion of bulk data: 
  function=bulk ./etcd_to_k8s.sh client/testdata/bulk_sync_k8s.json

Example conversion of watch data:
  ./etcd_to_k8s.sh testdata/k8s_vmi_list_map_prop_p1.json 

EOF
exit 0
fi

libdir="$(readlink -f "$(dirname "$0")")"
# change value to "bulk" to convert bulk data
function=watch
jq -L "$libdir" "import \"etcd_to_k8s\" as lib; lib::${function}_etcd_to_k8s" "$@"
