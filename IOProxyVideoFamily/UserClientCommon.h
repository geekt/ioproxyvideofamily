/*
 *  UserClientCommon.h
 *  IOProxyFramebuffer
 *
 *  Created by thrust on 061203.
 *  Copyright 2006 __MyCompanyName__. All rights reserved.
 *
 */

#ifndef _UserClientCommon_
#define _UserClientCommon_


typedef struct ConnectionStruct {
	UInt8			enabled;
	char			name[256];
	UInt32			width;
	UInt32			height;
	UInt32			maxWidth;
	UInt32			maxHeight;
} ConnectionStruct;

// these are the User Client method names    
enum {
    kMyUserClientOpen,
    kMyUserClientClose,
	kConnectionCountMethod,
	kConnectionPropertiesMethod,
	kSetConnectionPropertiesMethod,
	kApplyChangesMethod,
    kNumberOfMethods
};


#endif