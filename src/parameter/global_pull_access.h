#pragma once
#include "../utils/all.h"
#include "../transfer/transfer.h"
#include "../cluster/message_classes.h"
#include "../cluster/hashfrag.h"
#include "param.h"
namespace swift_snails {
/**
 * @brief pull parameter from remote Server
 *
 * @param Key key
 * @param Val local parameter type
 * @param Grad local gradient type
 */
template<class Key, class Val, class Grad>
class GlobalPullAccess : public VirtualObject {

public:
    typedef Key key_t;
    typedef Val val_t;
    typedef Grad grad_t;
    // pull key-value
    typedef std::pair<key_t, val_t> pull_val_t;
    typedef LocalParamCache<key_t, val_t, grad_t> param_cache_t;

    GlobalPullAccess() : 
        gtransfer(global_worker().transfer())
    { }

    void pull_with_barrier( 
        const std::unordered_set<key_t> &keys,
        param_cache_t &param_cache
    ) {
        StateBarrier barrier;
        std::atomic<size_t> num_reqs{0};
        std::map<int, std::vector<pull_val_t> > node_reqs;
        num_reqs = arrange_local_vals(keys, node_reqs);

        voidf_t extra_rsp_callback = [&barrier, &num_reqs] {
            if(-- num_reqs == 0) {
                barrier.set_state_valid();
                barrier.try_unblock();
            }
        };
        send(node_reqs, param_cache, extra_rsp_callback);
        barrier.block();
    }

protected:
    size_t arrange_local_vals(
        const std::unordered_set<key_t> &keys, 
        std::map<int, std::vector<pull_val_t> > &node_reqs) {
        pull_val_t param;    // empty value

        for(const auto& key : keys ) {
            int node_id = global_hashfrag<key_t>().to_node_id(key);
            if(node_reqs.count(node_id) == 0) {
                node_reqs[node_id] = std::move(std::vector<pull_val_t>());
            }

            param.first = key;
            node_reqs[node_id].push_back(param);
        }
        return node_reqs.size();
    }
    /*
     * @extra_rsp_callback will be called after 
     * send()'s response_recall_back finished
     */
    void send(
        std::map<int, std::vector<pull_val_t>> &items,
        param_cache_t &param_cache,
        voidf_t extra_rsp_callback = voidf_t()
        ) 
    {
       for( auto& item : items) {
            int node_id = item.first;
            auto &values = item.second;
            //LOG(INFO) << "to send to " << node_id;
            Request req;
            req.meta.message_class = WORKER_PULL_REQUEST;
            for(auto& value : values) {
                req.cont << value.first;
                req.cont << value.second;
            }
            // get remote parameters
            // rewrite to local cache
            req.call_back_handler = [this, &param_cache, extra_rsp_callback](std::shared_ptr<Request> rsp) {
                key_t key;
                val_t val;
                // write local cache 
                auto& params = param_cache.params();
                auto& grads = param_cache.grads();
                // TODO put rwlock inside? 
                { rwlock_write_guard lk (param_cache.rwlock());
                    while(! rsp->cont.read_finished()) {
                        rsp->cont >> key;
                        rsp->cont >> val;
                        params[key] = std::move(val);
                        // reset grads
                        grads[key] = grad_t();
                    }
                }

                if(extra_rsp_callback) extra_rsp_callback();
            };

            //RAW_LOG(INFO, "send pull req to %d", node_id);
            gtransfer.send(std::move(req), node_id);
            //recv_parcel->send(node_id);
        }
    }

private:
    //param_cache_t &param_cache; 
    Transfer<ServerWorkerRoute>& gtransfer; 
};  // class GlobalPullAccess


template <class Key, class Val, class Grad>
GlobalPullAccess<Key, Val, Grad>& global_pull_access() {
    static GlobalPullAccess<Key, Val, Grad> access;
    return access;
}

};  // end namespace swift_snails
