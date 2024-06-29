//
//  SyphonNSObject.mm
//  Syphon
//
//  Created by Tom Butterworth on 16/09/2022.
//

#include "SyphonNSObject.hpp"

SyphonNSObject::SyphonNSObject()
: mObject(nil)
{
    
}

SyphonNSObject::~SyphonNSObject()
{
    // This can be called from C++ with no autorelease pool in place, so we need one
    @autoreleasepool {
        // Do this explicitely here to force ARC release
        mObject = nil;
    }
}

SyphonNSObject::SyphonNSObject(const SyphonNSObject &o)
: mObject(o.mObject)
{
    
}

SyphonNSObject & SyphonNSObject::operator=(const SyphonNSObject &o)
{
    if (&o != this)
    {
        // This can be called from C++ with no autorelease pool in place, so we need one
        @autoreleasepool {
            mObject = o.mObject;
        }
    }
    return *this;
}

SyphonNSObject::operator bool() const
{
    return mObject ? true : false;
}

SyphonNSObject SNOMake(SyphonNSObject::ObjectType obj)
{
    SyphonNSObject dst;
    dst.mObject = obj;
    return dst;
}

void SNOSet(SyphonNSObject &dst, SyphonNSObject::ObjectType obj)
{
    dst.mObject = obj;
}

SyphonNSObject::ObjectType SNOGet(const SyphonNSObject &o)
{
    return o.mObject;
}
