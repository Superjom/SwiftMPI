//
//  common.h
//  SwiftSnails
//
//  Created by Chunwei on 12/2/14.
//  Copyright (c) 2014 Chunwei. All rights reserved.
//
#ifndef SwiftSnails_utils_common_h
#define SwiftSnails_utils_common_h

#define NDEBUG

#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <map>
#include <set>
#include <unordered_set>
#include <vector>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <memory>
#include <queue>
#include <thread>
#include <random>
#include <fstream>
#include <sstream>
#include "VirtualObject.h"
#include "glog/logging.h"
#include "glog/raw_logging.h"
#include "zmq.h"
#include "Eigen/Dense"

namespace swift_snails {
// common types
typedef unsigned char   byte_t;
typedef short           int16_t;
typedef unsigned short  uint16_t;
typedef int             int32_t;
typedef unsigned int    uint32_t;
typedef long long       int64_t;
typedef unsigned long long   uint64_t;
typedef uint32_t        index_t;

typedef std::function<void()> voidf_t;

// for repeat patterns
#define SS_REPEAT1(X) SS_REPEAT_PATTERN(X)
#define SS_REPEAT2(X, args...) SS_REPEAT_PATTERN(X) SS_REPEAT1(args)
#define SS_REPEAT3(X, args...) SS_REPEAT_PATTERN(X) SS_REPEAT2(args)
#define SS_REPEAT4(X, args...) SS_REPEAT_PATTERN(X) SS_REPEAT3(args)
#define SS_REPEAT5(X, args...) SS_REPEAT_PATTERN(X) SS_REPEAT4(args)
#define SS_REPEAT6(X, args...) SS_REPEAT_PATTERN(X) SS_REPEAT5(args)

inline std::mutex& global_fork_mutex() {
    static std::mutex mutex;
    return mutex;
}

// threadsafe popen pclose
inline FILE* guarded_popen(const char* command, const char* type) {
    std::lock_guard<std::mutex> lock(global_fork_mutex());
    return popen(command, type);
}

inline int guarded_pclose(FILE* stream) {
    std::lock_guard<std::mutex> lock(global_fork_mutex());
    return pclose(stream);
}


//typedef Eigen::Matrix<double, Eigen::Dynamic, 1> Vec;

};

#endif

