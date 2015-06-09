#pragma once
#include "../../utils/all.h"
namespace swift_snails {

/**
 * \brief shard of SparseTable
 *
 * a SparseTable contains several shards and the key-values will be 
 * splitted to the shards.
 *
 * SparseTable use shards to improve the efficiency of Read-Write Lock.
 */
template<typename Key, typename Value> 
struct alignas(64) SparseTableShard : public VirtualObject {
public:
    typedef Key     key_t;
    typedef Value   value_t;
    typedef dense_hash_map<key_t, value_t> map_t;

    SparseTableShard() {
        data().set_empty_key(std::numeric_limits<key_t>::max());
    }

    bool find(const key_t& key, value_t* &val) {
        rwlock_read_guard lock(_rwlock);
        auto it = data().find(key);
        if (it == data().end()) return false;
        val = &(it->second);
        return true;
    }
    bool find(const key_t& key, value_t &val) {
        rwlock_read_guard lock(_rwlock);
        auto it = data().find(key);
        if (it == data().end()) return false;
        val = it->second;
        return true;
    }

    void assign(const key_t& key, const value_t &val) {
        rwlock_write_guard lock(_rwlock);
        data()[key] = val; 
    }

    index_t size() {
        rwlock_read_guard lock(_rwlock);
        return data().size();
    }
    void set_shard_id( int x) {
        CHECK_GE(x, 0);
        _shard_id =  x;
    }
    int shard_id() const {
        return _shard_id;
    }
    /**
     * \brief output parameters to ostream
     * \warning should define value's output method first
     */
    friend std::ostream& operator<< (std::ostream& os, SparseTableShard &shard)
    {
        rwlock_read_guard lk(shard._rwlock);
        for(auto& item : shard.data() ) {
            os << item.first << "\t";
            os << item.second << std::endl;
        }
        return os;
    }

protected:
    // not thread safe!
    map_t& data() {
        return _data;
    }


private:
    map_t _data;
    int _shard_id = -1;
    RWLock _rwlock;
    //mutable std::mutex _mutex;
};  // struct SparseTableShard
/**
 * \brief container of sparse parameters
 *
 * a SparseTable has several shards to split the storage and operation of 
 * parameters.
 */
template<typename Key, typename Value>
class SparseTable : public VirtualObject {
public:
    typedef Key     key_t;
    typedef Value   value_t;
    typedef SparseTableShard<key_t, value_t> shard_t;

    SparseTable() {
        _shard_num = global_config().get_config("server", "shard_num").to_int32();
        _shards.reset(new shard_t[shard_num()]);
    }

    shard_t &shard(int shard_id) {
        return _shards[shard_id];
    }

    bool find(const key_t &key, value_t* &val) {
        int shard_id = to_shard_id(key);
        return shard(shard_id).find(key, val);
    }

    bool find(const key_t& key, value_t &val) {
        int shard_id = to_shard_id(key);
        return shard(shard_id).find(key, val);
    }

    void assign (const key_t& key, const value_t &val) {
        int shard_id = to_shard_id(key);
        shard(shard_id).assign(key, val);
    }
    /**
     * output parameters to ostream
     */
    void output() {
        for(int i = 0; i < shard_num(); i++) {
            std::cout << shard(i);
        }
    }

    index_t size() const {
        index_t res = 0;
        for(int i = 0; i < shard_num(); i ++) {
            auto& shard = _shards[i];
            res += shard.size();
        }
        return res;
    }
    // TODO assign protected
    int to_shard_id(const key_t& key) {
        return get_hash_code(key)  % shard_num();
    }
    int shard_num() const {
        return _shard_num;
    }

private:
    std::unique_ptr<shard_t[]> _shards; 
    int _shard_num = 1;
};  // class SparseTable
/**
 * \brief Server-side operation agent
 *
 * Pull: worker parameter query request.
 *
 * \param Table subclass of SparseTable
 * \param AccessMethod Server-side operation on parameters
 */
template<typename Table, typename AccessMethod>
class PullAccessAgent {
public:
    typedef Table          table_t;
    typedef typename Table::key_t   key_t;
    typedef typename Table::value_t value_t;

    typedef AccessMethod   access_method_t;
    typedef typename AccessMethod::pull_val_t pull_val_t;
    typedef typename AccessMethod::pull_param_t pull_param_t;

    explicit PullAccessAgent() {
    }
    void init(table_t& table)
    {
        _table = &table;
    }

    explicit PullAccessAgent(table_t& table) :
        _table(&table)
    { }

    int to_shard_id(const key_t& key) {
        return _table->to_shard_id(key);
    }
    /**
     * Server-side query parameter
     */
    void get_pull_value(const key_t& key, pull_val_t &val) {
        pull_param_t param;
        if (! _table->find(key, param)) {
            _access_method.init_param(key, param);
            _table->assign(key, param);
        }
        _access_method.get_pull_value(key, param, val);
    }
    /**
     * \brief Worker-side get pull value
     */
    void apply_pull_value(const key_t &key, pull_param_t &param, const pull_val_t& val) {
        _access_method.apply_pull_value(key, param, val);
    }
private:
    table_t     *_table;
    AccessMethod _access_method;
};  // class AccessAgent

/**
 * \brief Server-side push agent
 */
template<typename Table, typename AccessMethod>
class PushAccessAgent {
public:
    typedef Table          table_t;
    typedef typename Table::key_t   key_t;
    typedef typename Table::value_t value_t;

    typedef typename AccessMethod::push_val_t push_val_t;
    typedef typename AccessMethod::push_param_t push_param_t;

    explicit PushAccessAgent() {
    }
    void init(table_t& table) {
        _table = &table;
    }

    explicit PushAccessAgent(table_t& table) : \
        _table(&table) 
    { }
    /**
     * \brief update parameters with the value from remote worker nodes
     */
    void apply_push_value(const key_t& key, const push_val_t& push_val)
    {
        push_param_t *param = nullptr;
        // TODO improve this in fix mode?
        CHECK( _table->find(key, param) ) << "new key should be inited before";
        CHECK_NOTNULL(param);
        /*
        DLOG(INFO) << "to apply push val: key:\t" << key 
                   << "\tparam\t" << *param 
                   << "\tpush_val\t" << push_val;
        */
        _access_method.apply_push_value(key, *param, push_val);
    }

private:
    table_t         *_table = nullptr;
    AccessMethod    _access_method;

};  // class PushAccessAgent


template<class Key, class Value>
SparseTable<Key, Value>& global_sparse_table() {
    static SparseTable<Key, Value> table;
    return table;
}

template<typename Table, typename AccessMethod>
auto make_pull_access(Table &table)
-> std::unique_ptr< PullAccessAgent<Table, AccessMethod>>
{
    AccessMethod method;
    std::unique_ptr<PullAccessAgent<Table, AccessMethod>> res(new PullAccessAgent<Table, AccessMethod>(table));
    return std::move(res);
}


template<typename Table, typename AccessMethod>
auto make_push_access(Table &table)
-> std::unique_ptr< PushAccessAgent<Table, AccessMethod>>
{
    AccessMethod method;
    std::unique_ptr<PushAccessAgent<Table, AccessMethod>> res(new PushAccessAgent<Table, AccessMethod>(table));
    return std::move(res);
}

};  // end namespace swift_snails
