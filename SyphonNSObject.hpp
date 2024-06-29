//
//  SyphonNSObject.hpp
//  ofxSyphon
//
//  Created by Tom Butterworth on 16/09/2022.
//

#ifndef SyphonNSObject_hpp
#define SyphonNSObject_hpp

#ifdef __OBJC__
#include <Foundation/Foundation.h>
#endif

class SyphonNSObject {
public:
#ifdef __OBJC__
    using ObjectType = id<NSObject>;
#else
    using ObjectType = void *;
#endif
    SyphonNSObject();
    ~SyphonNSObject();
    SyphonNSObject(const SyphonNSObject &o);
    SyphonNSObject & operator=(const SyphonNSObject &o);
    operator bool() const;
    // There is no Objective-C functionality on the class
    // so it is logically impossible to cause undefined behaviour from pure C++ code
    friend SyphonNSObject SNOMake(SyphonNSObject::ObjectType obj);
    friend void SNOSet(SyphonNSObject &dst, SyphonNSObject::ObjectType obj);
    friend SyphonNSObject::ObjectType SNOGet(const SyphonNSObject &o);
private:
    ObjectType mObject;
};

#ifdef __OBJC__
SyphonNSObject SNOMake(SyphonNSObject::ObjectType obj);
void SNOSet(SyphonNSObject &dst, SyphonNSObject::ObjectType obj);
SyphonNSObject::ObjectType SNOGet(const SyphonNSObject &o);
#endif

#endif /* SyphonNSObject_hpp */
