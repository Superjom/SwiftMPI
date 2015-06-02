//
//  SpinLock.h
//  SwiftSnails
//
//  Created by Chunwei on 12/2/14.
//  Copyright (c) 2014 Chunwei. All rights reserved.
//

#ifndef SwiftSnails_utils_SpinLock_h
#define SwiftSnails_utils_SpinLock_h
#include "common.h"
#include <unistd.h>
#include <sys/syscall.h>
#include <atomic>
#include <pthread.h>

namespace swift_snails {

/*
// 封装pthread_spinlock
class SpinLock : public VirtualObject {
public:
    SpinLock() {
        PCHECK( 0 == pthread_spin_init(&_spin, 0));    
    }
    ~SpinLock() {
        PCHECK( 0 == pthread_spin_destroy(&_spin));
    }
    void lock() {
        PCHECK( 0 == pthread_spin_lock(&_spin));
    }
    void unlock() {
        PCHECK( 0 == pthread_spin_unlock(&_spin));
    }

private:
    pthread_spinlock_t _spin;
};

*/
 
class SpinLock
{
public:
    void lock()
    {
        while(lck.test_and_set(std::memory_order_acquire))
        {}
    }
 
    void unlock()
    {
        lck.clear(std::memory_order_release);
    }
 
private:
    std::atomic_flag lck = ATOMIC_FLAG_INIT;
};


}; // end namespace swift_snails

#endif
