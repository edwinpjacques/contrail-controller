# Filter to generate Kubernetes JSON data from ETCD JSON data.

# Convert type_name into CamelCase
def type_case:
    . | split("_") | map((.[0:1] | ascii_upcase) + .[1:]) | join("");

# Convert field_name into camelCase
def field_case:
    . | type_case | (.[0:1] | ascii_downcase) + .[1:];

# For a given value key/value, recurse to fix the field names
def recurse_field_case:
    . | with_entries(.key |= (. | field_case))
      | with_entries(.value |= if (. | type == "object") then (. | recurse_field_case) else . end);

# Create key/value pairs for all fields other than these.
# fq_name
# id_perms
# perms2
# type
# uuid
def spec_data:
    . | del(.fq_name) | del(.id_perms) | del(.perms2) | del(.type) | del(.uuid) | recurse_field_case;

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
]