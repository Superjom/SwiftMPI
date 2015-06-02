//
//  VirtualObject.h
//  SwiftSnails
//
//  Created by Chunwei on 12/2/14.
//  Copyright (c) 2014 Chunwei. All rights reserved.
//

#ifndef SwiftSnails_utils_VirtualObject_h
#define SwiftSnails_utils_VirtualObject_h
namespace swift_snails {

// can not be copied or assigned from other object 
class VirtualObject {
public:
    VirtualObject() = default;
    VirtualObject(const VirtualObject &) = delete;
    VirtualObject &operator= (const VirtualObject &) = delete;
    ~VirtualObject() = default;
};



}; // end namespace swift_snails


#endif
