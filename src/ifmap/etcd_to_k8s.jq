# Purpose: Filter to generate Kubernetes JSON data from ETCD JSON data.
# Author:  Edwin P. Jacques (ejacques@juniper.net)
# Copyright (c) 2020 Juniper Networks, Inc. All rights reserved.

# Convert type_name into CamelCase
def type_case:
    . | split("_") 
      | map((.[0:1] | ascii_upcase) + .[1:]) 
      | join("");

# Convert field_name into camelCase
def field_case:
    . | type_case 
      | (.[0:1] | ascii_downcase) + .[1:];

# For a given value key/value, recurse to fix the field names
def recurse_field_case:
    . | with_entries(.key |= (. | field_case))
      | with_entries(.value |= 
            if (. | type == "object") 
                then (. | recurse_field_case) 
                else . 
            end);

# Create key/value pairs for all fields in the spec.
def spec_data:
    . | 
        (
          select (.parent_uuid != null) 
          | { "parent": {
                "apiVersion": "core.contrail.juniper.net/v1alpha1",
                "kind": .parent_type | type_case,
                "uuid": .parent_uuid
              }
            }
        ),
        del(.fq_name) 
      | del(.id_perms) 
      | del(.perms2) 
      | del(.type) 
      | del(.uuid)
      | del(.parent_type)
      | del(.parent_uuid)
      | del(.parent_name)
      | del(.event)
      | recurse_field_case;

# Convert etcd database event list into a list of Kubernetes watch objects.
def watch_etcd_to_k8s:
[
    .[] | 
          (
            .event | split("/")
            |
              if (.[1] == "PAUSED") then
                {"type": .[1]} 
              else
                {"type": .[1]} 
                + {"object": (
                    {"apiVersion": "core.contrail.juniper.net/v1alpha1"}
                  + {"kind": .[2] | type_case} 
                  + {"metadata": (
                      {"uid": .[3]}
                    )}
                  )}
              end
            )
              + if (.event == "/PAUSED/") then 
                  null
                else 
                  { "spec": . | spec_data } 
                end
];

# Convert etcd database state document into a list of Kubernetes objects.
def bulk_etcd_to_k8s:
[
    .[]
        | { "apiVersion": "core.contrail.juniper.net/v1alpha1" }
        + { "kind": .type | type_case }
        + { "metadata": {
              "creationTimestamp": .id_perms.last_modified,
              "generation": 1,
              "name": .fq_name[-1],
              "resourceVersion": 1,
              "uuid": .uuid
            }
          }
        + { "spec": . | spec_data }
        + { "status": {
                "fqName": .fq_name,
                "state": "Success"
            }
          }
];