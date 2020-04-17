/*
 * Copyright (c) 2018 Juniper Networks, Inc. All rights reserved.
 */
#ifndef ctrlplane_config_k8s_client_test_h
#define ctrlplane_config_k8s_client_test_h

#include <boost/foreach.hpp>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>
#include "config-client-mgr/config_cass2json_adapter.h"
#include "config-client-mgr/config_client_manager.h"
#include "config-client-mgr/config_k8s_client.h"
#include "config-client-mgr/config_factory.h"
#include "database/etcd/eql_if.h"
#include "ifmap/client/config_json_parser.h"
#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"

using namespace std;
using etcd::etcdql::EtcdIf;
using etcd::etcdql::EtcdResponse;
using etcd::etcdql::WatchAction;
using contrail_rapidjson::StringBuffer;
using contrail_rapidjson::Writer;

static int num_bunch = 8;
static int max_yield = 4;
static Document dbDoc_;
static int db_index = 2;

class EqlIfTest : public EtcdIf {
public:
    EqlIfTest(const vector<string> &etcd_hosts,
              const int port,
              bool useSsl) : EtcdIf(etcd_hosts,
                                   port,
                                   useSsl) {
    }

    virtual bool Connect() { return true; }

    virtual EtcdResponse Get(const string& key,
                             const string& range_end,
                             int limit) {
        EtcdResponse resp;
        EtcdResponse::kv_map kvs;

        if (--db_index == 0) {
            resp.set_err_code(-1);
            return resp;
        }

        /**
          * Make sure the database document is populated.
          * Set error code to 0
          */
        resp.set_err_code(0);

        /* Bulk data is one big document with an "items" member. */
        Value& items = dbDoc_["items"];

        for (Value::ConstValueIterator itr = items.Begin();
             itr != items.End(); itr++) {
                 
            Value::ConstMemberIterator metadata = itr->FindMember("metadata");
            Value::ConstMemberIterator uuid = metadata->value.GetObject().FindMember("uid");
            
            /**
              * Get the uuid string and the value from the
              * database Document created from the input file.
              */
            string uuid_str = uuid->value.GetString();
            StringBuffer sb;
            Writer<StringBuffer> writer(sb);
            itr->Accept(writer);
            string value_str = sb.GetString();

            /**
              * Populate the key-value store to be saved
              * in the K8sResponse.
              * 
              * In theory there can be more than one entry for the same UUID,
              * so we store the data into a multimap.
              */
            kvs.insert(pair<string, string> (uuid_str, value_str));
        }

        /**
          * Save the kvs in the EtcdResponse and return
          * to caller.
          */
        resp.set_kv_map(kvs);

        return resp;
    }

    static void ParseDatabase(string db_file) {
        string json_db = FileRead(db_file);
        assert(json_db.size() != 0);

        Document *dbDoc = &dbDoc_;
        dbDoc->Parse<0>(json_db.c_str());
        if (dbDoc->HasParseError()) {
            size_t pos = dbDoc->GetErrorOffset();
            // GetParseError returns const char *
            std::cout << "Error in parsing JSON DB at "
                << pos << "with error description"
                << dbDoc->GetParseError()
                << std::endl;
            exit(-1);
        }
        task_util::WaitForIdle();
    }

    static string FileRead(const string &filename) {
        ifstream file(filename.c_str());
        string content((istreambuf_iterator<char>(file)),
                       istreambuf_iterator<char>());
        return content;
    }
};

class ConfigK8sClientTest : public ConfigK8sClient {
public:
    ConfigK8sClientTest(
             ConfigClientManager *mgr,
             EventManager *evm,
             const ConfigClientOptions &options,
             int num_workers) :
                   ConfigK8sClient(mgr,
                                    evm,
                                    options,
                                    num_workers),
                   cevent_(0) {
    }

    Document *ev_load() { return &evDoc_; }

    string GetJsonValue(size_t index) 
    {
        string value_str;

        // Each event is a nested document.  Skip to the next one to be read.
        Value::Array events = evDoc_.GetArray();

        // trivial case, simply return
        if (events.Empty()) {
            return value_str;
        }

        // make sure index is not out of range
        if (events.Size() <= index) {
            return value_str;
        }

        Value &val = events[index];
        StringBuffer sb;
        Writer<StringBuffer> writer(sb);
        val.Accept(writer);
        value_str = sb.GetString();
        return (value_str);
    }

    // All the test data is an array of documents.
    void ParseEventsJson(string events_file) {
        string json_events = FileRead(events_file);
        assert(json_events.size() != 0);

        Document *eventDoc = &evDoc_;
        eventDoc->Parse<0>(json_events.c_str());
        if (eventDoc->HasParseError()) {
            size_t pos = eventDoc->GetErrorOffset();
            // GetParseError returns const char *
            std::cout << "Error in parsing JSON events at "
                << pos << "with error description"
                << eventDoc->GetParseError()
                << std::endl;
            exit(-1);
        }
    }

    void FeedEventsJson() {
        EtcdResponse resp;
        resp.set_err_code(0);

        // Each event is a nested document.  Skip to the next one to be read.
        Value::Array events = evDoc_.GetArray();

        // trivial case, simply return
        if (events.Empty()) {
            return;
        }

        while (cevent_ < events.Size())
        {
            /**
              * Get the uuid string and the value from the
              * database Document created from the input file.
              */
            Value event = events[cevent_++].GetObject();
            Value::MemberIterator type = event.FindMember("type");
            string type_str = type->value.GetString();

            Value::MemberIterator object = event.FindMember("object");
            Value::MemberIterator metadata;
            Value::MemberIterator uid;
            string uid_str;

            if (object != event.MemberEnd()) {
                metadata = object->value.FindMember("metadata");
                uid = metadata->value.FindMember("uid");
                uid_str = uid->value.GetString();
            }

            if (type_str == "ADDED") {
                resp.set_action(WatchAction(0));
            } else if (type_str == "MODIFIED") {
                resp.set_action(WatchAction(1));
            } else if (type_str == "DELETED") {
                resp.set_action(WatchAction(2));
            } else if (type_str == "PAUSED") {
                break;
            }
            assert((resp.action() >= WatchAction(0)) &&
                   (resp.action() <= (WatchAction(2))));
            resp.set_key(uid_str);

            StringBuffer sb;
            Writer<StringBuffer> writer(sb);
            object->value.Accept(writer);
            string value_str = sb.GetString();
            resp.set_val(value_str);
            
            ConfigK8sClient::ProcessResponse(resp);
        }
        task_util::WaitForIdle();
    }

    static string FileRead(const string &filename) {
        return (EqlIfTest::FileRead(filename));
    }

private:
    virtual uint32_t GetNumUUIDRequestToBunch() const {
        return num_bunch;
    }

    virtual const int GetMaxRequestsToYield() const {
        return max_yield;
    }

    Document evDoc_;
    size_t cevent_;
};

class ConfigClientManagerMock : public ConfigClientManager {
public:
    ConfigClientManagerMock(
                   EventManager *evm,
                   string hostname,
                   string module_name,
                   const ConfigClientOptions& config_options) :
             ConfigClientManager(evm,
                                 hostname,
                                 module_name,
                                 config_options) {
    }
};
#endif
