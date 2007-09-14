/*
 *  IOProxyVideoCard.cpp
 *  IOProxyVideoFamily
 *
 *  Created by thrust on 061213.
 *
 *	based in part on OpenControlFramebuffer and VirtualFramebuffer by
 *	Ryan Rempel at Other World Computing
 *
 *  Copyright (c) 2006, Ritchie Argue
 *  All rights reserved.
 *  
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *
 *  - Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *  - Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *  - Neither the name of the <ORGANIZATION> nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *
 */


#include <IOKit/IOLib.h>
#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/IOWorkLoop.h>

#include "IOProxyVideoCard.h"
#include "IOProxyFramebuffer.h"


//extern "C" {
//#include <pexpert/pexpert.h>//This is for debugging purposes ONLY
//}

// Define my superclass
#define super IOService

// REQUIRED! This macro defines the class's constructors, destructors,
// and several other methods I/O Kit requires. Do NOT use super as the
// second parameter. You must use the literal name of the superclass.
OSDefineMetaClassAndStructors(com_doequalsglory_driver_IOProxyVideoCard, IOService)


// External OSSymbol Cache, they have to be somewhere.
const OSSymbol *gIODVCHeadListKey = 0;
const OSSymbol *gIODVCHeadIndexKey = 0;
const OSSymbol *gIODVCHeadEnabledKey = 0;
const OSSymbol *gIODVCHeadNameKey = 0;
const OSSymbol *gIODVCHeadWidthKey = 0;
const OSSymbol *gIODVCHeadHeightKey = 0;
const OSSymbol *gIODVCHeadMaxWidthKey = 0;
const OSSymbol *gIODVCHeadMaxHeightKey = 0;
const OSSymbol *gIODVCHeadDefaultName = 0;

class IOProxyVideoCardGlobals {	
public:
	IOProxyVideoCardGlobals();
    ~IOProxyVideoCardGlobals();
};

// Create an instance of the IOProxyVideoCardGlobals
// This runs the static constructor and destructor so that
// I can grab and release a lock as appropriate.
static IOProxyVideoCardGlobals dvcGlobals;

#define OSSYM(str) OSSymbol::withCStringNoCopy(str)
IOProxyVideoCardGlobals::IOProxyVideoCardGlobals() {
	gIODVCHeadListKey		= OSSYM("com_doequalsglory_driver_HeadList");
	
	gIODVCHeadIndexKey		= OSSYM("index");
	gIODVCHeadEnabledKey	= OSSYM("enabled");
	gIODVCHeadNameKey		= OSSYM("name");
	gIODVCHeadWidthKey		= OSSYM("width");
	gIODVCHeadHeightKey		= OSSYM("height");
	gIODVCHeadMaxWidthKey	= OSSYM("maxWidth");
	gIODVCHeadMaxHeightKey	= OSSYM("maxHeight");
	
	gIODVCHeadDefaultName	= OSSYM("ProxyVideoHead");
}
#undef OSSYM

#define SAFE_RELEASE(x) do { if (x) x->release(); x = 0; } while(0)
IOProxyVideoCardGlobals::~IOProxyVideoCardGlobals() {
    SAFE_RELEASE(gIODVCHeadListKey);
	
	SAFE_RELEASE(gIODVCHeadIndexKey);
	SAFE_RELEASE(gIODVCHeadEnabledKey);
	SAFE_RELEASE(gIODVCHeadNameKey);
	SAFE_RELEASE(gIODVCHeadWidthKey);
	SAFE_RELEASE(gIODVCHeadHeightKey);
	SAFE_RELEASE(gIODVCHeadMaxWidthKey);
	SAFE_RELEASE(gIODVCHeadMaxHeightKey);
	
	SAFE_RELEASE(gIODVCHeadDefaultName);
}
#undef SAFE_RELEASE


#pragma mark -
#pragma mark IOProxyVideoCard

bool
com_doequalsglory_driver_IOProxyVideoCard::init(OSDictionary *dict) {
    bool res = super::init(dict);
    IOLog("IOProxyVideoCard::init()\n");
    return res;
}


void
com_doequalsglory_driver_IOProxyVideoCard::free(void) {
    IOLog("IOProxyVideoCard::free()\n");
    super::free();
}


IOService *
com_doequalsglory_driver_IOProxyVideoCard::probe(IOService *provider, SInt32
												   *score) {
    IOService *res = super::probe(provider, score);
    IOLog("IOProxyVideoCard::probe()\n");
    return res;
}


bool
com_doequalsglory_driver_IOProxyVideoCard::start(IOService *provider) {
    bool res = super::start(provider);
    IOLog("IOProxyVideoCard::start()\n");
	
	/* publish ourselves so clients can find us. this is not necessary if the
		Info.plist contains an IOUserClientClass key. */
    registerService();
	
	// set up some things to make it easier to see us in IORegExplorer
	setName("DEG,ProxyVideoCard");												// must use setName(), not just setProperty("name","foo");

	// Get the master nub creation table from our properties
	// BTW there is no need to do a copyProperty as we can rely on our properties
	// being valid until we return from start, after that all bets are basically off though
	// you have been warned.
	OSArray *nubPropertyList = OSDynamicCast(OSArray, getProperty(gIODVCHeadListKey));
	if (!nubPropertyList) {
		IOLog("IOProxyVideoCard::start() - error: no Head Array in personality\n");
		
		return res;																// Nothing more to do just return success
	}
	
	//nubList = OSArray::withCapacity(nubPropertyList->getCount());				// new array to hold our nubs
	int nubIndex = 0;
	char nubLocation[3];														// max "99\0"
	
	OSCollectionIterator *nubIter = OSCollectionIterator::withCollection(nubPropertyList);
	OSDictionary *nubProperties;
	
	while ((nubProperties = (OSDictionary *) nubIter->getNextObject())) {
		if (OSDynamicCast(OSDictionary, nubProperties)) {
			com_doequalsglory_driver_IOProxyVideoHead *nub = OSTypeAlloc(com_doequalsglory_driver_IOProxyVideoHead);

			if (nub) {
				if (!nub->init(nubProperties) || !nub->attach(this)) {
					nub->release();
					continue;													// Probably should log the failure too
				}
				
				nub->setProperty(gIODVCHeadIndexKey->getCStringNoCopy(), nubIndex, 32);
				
				snprintf(nubLocation, sizeof(nubLocation), "%X", nubIndex++);
				
				// set nub location
				nub->setLocation(nubLocation);
				
				// We now have a valid nub that is connected into the service plane. time to start matching
				nub->registerService();											// That's it - matching has started now
				
//				IOLog("IOProxyVideoCard::start() added nub %X\n", nub);
//				
//				if (!nubList->setObject(nub)) {
//					IOLog("failed to add nub\n");
//				}
			}
		}
	}
	nubIter->release();

	return res;
}


void
com_doequalsglory_driver_IOProxyVideoCard::stop(IOService *provider) {
    IOLog("IOProxyVideoCard::stop()\n");
	
	OSIterator *clientIter = getClientIterator();
	IOService *client;
	
	while ((client = (IOService *) clientIter->getNextObject())) {
		IOLog("releasing client: %x\n", client);
		client->terminate(kIOServiceRequired | kIOServiceTerminate | kIOServiceSynchronous);
		//client->terminate();													// is this all we need to do?
		client->release();														// no, need to release() as well
	}
	clientIter->release();
		
    super::stop(provider);
}


IOWorkLoop *
com_doequalsglory_driver_IOProxyVideoCard::getWorkLoop() {
	if ((vm_address_t) cntrlSync >> 1) {
		return cntrlSync;
	}
	
	if (OSCompareAndSwap(0, 1, (UInt32 *) &cntrlSync)) {
		cntrlSync = IOWorkLoop::workLoop();
		
	} else {
		while (cntrlSync == (IOWorkLoop *) 1) {
			thread_block(0);
		}
	}
	return cntrlSync;
}
	

#pragma mark user client interface

IOReturn
com_doequalsglory_driver_IOProxyVideoCard::connectionCount(int *outCount) {
	// If the user client's 'open' method was never called, return kIOReturnNotOpen.
    if (!isOpen()) {
        return kIOReturnNotOpen;
    }
	
	// count heads
	OSIterator *clientIter = getClientIterator();
	IOService *client;
	*outCount = 0;
	while ((client = (IOService *) clientIter->getNextObject())) {
		com_doequalsglory_driver_IOProxyVideoHead *nub =
		OSDynamicCast(com_doequalsglory_driver_IOProxyVideoHead, client);
		if (nub) {
			(*outCount)++;
		}
	}
	clientIter->release();
	
	//*outCount = nubList->getCount();
	
    IOLog("IOProxyVideoCard::connectionCount(*outCount=%d)\n", *outCount);
    
    return kIOReturnSuccess;
}


IOReturn
com_doequalsglory_driver_IOProxyVideoCard::connectionProperties(int inIndex, ConnectionStruct *outStruct, IOByteCount *outStructSize) {
	// If the user client's 'open' method was never called, return kIOReturnNotOpen.
    if (!isOpen()) {
        return kIOReturnNotOpen;
	}
	
	IOLog("IOProxyVideoCard::connectionProperties(inIndex=%d)\n", inIndex);
	
	OSIterator *clientIter = getClientIterator();
	IOService *client;
	com_doequalsglory_driver_IOProxyVideoHead *nub = NULL;
	
	while ((client = (IOService *) clientIter->getNextObject())) {
		com_doequalsglory_driver_IOProxyVideoHead *tempNub =
		OSDynamicCast(com_doequalsglory_driver_IOProxyVideoHead, client);
		if (tempNub) {
			OSNumber *index = OSDynamicCast(OSNumber, tempNub->getProperty(gIODVCHeadIndexKey));
			if (inIndex == index->unsigned32BitValue()) {
				nub = tempNub;
				continue;
			}
		}
	}
	clientIter->release();
	
	if (!nub) {
		IOLog("IOProxyVideoCard  error: no nub at index %d\n", inIndex);
		return kIOReturnInternalError;
	} else {
		OSBoolean *enabled = OSDynamicCast(OSBoolean, nub->getProperty(gIODVCHeadEnabledKey));
		if (enabled) {
			outStruct->enabled = enabled->getValue();
		} else {
			IOLog("IOProxyVideoCard  error: no enabled property at index %d\n", inIndex);
			return kIOReturnInternalError;
		}
		
		const char *name = nub->getName();										// name is not a property
		if (name) {
			snprintf(outStruct->name, sizeof(outStruct->name), "%s", name);
		} else {
			IOLog("IOProxyVideoCard  error: no name property at index %d\n", inIndex);
			return kIOReturnInternalError;
		}
		
		OSNumber *width = OSDynamicCast(OSNumber, nub->getProperty("width"));
		if (width) {
			outStruct->width = width->unsigned32BitValue();
		} else {
			IOLog("IOProxyVideoCard  error: no width property at index %d\n", inIndex);
			return kIOReturnInternalError;
		}
		
		OSNumber *height = OSDynamicCast(OSNumber, nub->getProperty("height"));
		if (height) {
			outStruct->height = height->unsigned32BitValue();
		} else {
			IOLog("IOProxyVideoCard  error: no height property at index %d\n", inIndex);
			return kIOReturnInternalError;
		}
		
		OSNumber *maxWidth = OSDynamicCast(OSNumber, nub->getProperty("maxWidth"));
		if (maxWidth) {
			outStruct->maxWidth = maxWidth->unsigned32BitValue();
		} else {
			IOLog("IOProxyVideoCard  error: no maxWidth property at index %d\n", inIndex);
			return kIOReturnInternalError;
		}
		
		OSNumber *maxHeight = OSDynamicCast(OSNumber, nub->getProperty("maxHeight"));
		if (maxHeight) {
			outStruct->maxHeight = maxHeight->unsigned32BitValue();
		} else {
			IOLog("IOProxyVideoCard  error: no maxHeight property at index %d\n", inIndex);
			return kIOReturnInternalError;
		}
	}
	
	*outStructSize = sizeof(ConnectionStruct);
	
	return kIOReturnSuccess;
}


IOReturn
com_doequalsglory_driver_IOProxyVideoCard::setConnectionProperties(int inIndex, ConnectionStruct *inStruct, IOByteCount *inStructSize) {
	// If the user client's 'open' method was never called, return kIOReturnNotOpen.
    if (!isOpen()) {
        return kIOReturnNotOpen;
	}
	
	OSIterator *clientIter = getClientIterator();
	IOService *client;
	com_doequalsglory_driver_IOProxyVideoHead *nub = NULL;
	
	while ((client = (IOService *) clientIter->getNextObject())) {
		com_doequalsglory_driver_IOProxyVideoHead *tempNub =
		OSDynamicCast(com_doequalsglory_driver_IOProxyVideoHead, client);
		if (tempNub) {
			OSNumber *index = OSDynamicCast(OSNumber, tempNub->getProperty(gIODVCHeadIndexKey));
			if (inIndex == index->unsigned32BitValue()) {
				nub = tempNub;
				continue;
			}
		}
	}
	clientIter->release();
	
	if (!nub) {
		IOLog("IOProxyVideoCard  error: no nub at index %d\n", inIndex);
		return kIOReturnInternalError;
	} else {
		IOLog("IOProxyVideoCard::connectionProperties(inIndex=%d)\n", inIndex);
		IOLog("IOProxyVideoCard  enabled=%d\n", inStruct->enabled);
		IOLog("IOProxyVideoCard     name=%s\n", inStruct->name);
		IOLog("IOProxyVideoCard    width=%d\n", inStruct->width);
		IOLog("IOProxyVideoCard   height=%d\n", inStruct->height);
		
		nub->setProperty(gIODVCHeadEnabledKey->getCStringNoCopy(), (bool)inStruct->enabled);
		nub->setName(inStruct->name);
		nub->setProperty(gIODVCHeadWidthKey->getCStringNoCopy(), inStruct->width, 32);
		nub->setProperty(gIODVCHeadHeightKey->getCStringNoCopy(), inStruct->height, 32);
	}
	
	return kIOReturnSuccess;
}


IOReturn
com_doequalsglory_driver_IOProxyVideoCard::applyChanges(void) {
	IOLog("IOProxyVideoCard::applyChanges()\n");
	
	// poke the framebuffers, generate a kIOFBConnectInterruptType interrupt
	// somehow?

	OSIterator *clientIter = getClientIterator();
	IOService *client;
	
	while ((client = (IOService *) clientIter->getNextObject())) {
		com_doequalsglory_driver_IOProxyVideoHead *nub =
			 OSDynamicCast(com_doequalsglory_driver_IOProxyVideoHead, client);
		if (nub) {
			IOLog("found video head %s\n", nub->getName());
			
			// tell the head we've got a change
			nub->messageClients(kIOFBConnectInterruptType, NULL, 0);
		}
	}
	clientIter->release();
	
	return kIOReturnSuccess;
}


#pragma mark nub management

//IOReturn 
//com_doequalsglory_driver_IOProxyVideoCard::getNubResources(IOService *nub) {
//	IOLog("IOProxyVideoCard::getNubResources()\n");
//	
////    if( nub->getDeviceMemory())
////		return( kIOReturnSuccess );
//	
//    //IODTResolveAddressing( nub, "reg", getProvider()->getDeviceMemoryWithIndex(0) );
//	
//    return kIOReturnSuccess;
//}


#pragma mark -
#pragma mark IOProxyVideoHead

#undef super
#define super IOService

OSDefineMetaClassAndStructors(com_doequalsglory_driver_IOProxyVideoHead, IOService);

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/*! ----------------------------+---------------+-------------------------------
	@method		init()
	@abstract	start isn't called on nubs it seems, so we do setup in init
	@discussion	if not enabled, don't even create?
	@param		<#name#> <#description#>
	@result		<#description#>
--------------------------------+---------------+---------------------------- */
bool
com_doequalsglory_driver_IOProxyVideoHead::init(OSDictionary *dict) {
    bool res = super::init(dict);
    IOLog("IOProxyVideoHead::init()\n");
	
	OSBoolean *enabled = OSDynamicCast(OSBoolean, dict->getObject(gIODVCHeadEnabledKey));
	if (!enabled) {
		IOLog("IOProxyVideoHead  enabled=0 (default)\n");
		
		//return false;
		setProperty(gIODVCHeadEnabledKey->getCStringNoCopy(), false);
	}
	
	OSString *name = OSDynamicCast(OSString, dict->getObject(gIODVCHeadNameKey));
	if (!name) {
		setName(gIODVCHeadDefaultName);
		IOLog("IOProxyVideoHead  name: ProxyVideoHead (default)\n");
	} else {
		IOLog("IOProxyVideoHead  name: %s\n", name->getCStringNoCopy());
		setName(name->getCStringNoCopy());
	}
	
	OSNumber *width = OSDynamicCast(OSNumber, dict->getObject(gIODVCHeadWidthKey));
	if (!width) {
		setProperty(gIODVCHeadWidthKey->getCStringNoCopy(), 1280, 32);
		IOLog("IOProxyVideoHead  width=1280 (default)\n");
	}
	
	OSNumber *height = OSDynamicCast(OSNumber, dict->getObject(gIODVCHeadHeightKey));
	if (!height) {
		setProperty(gIODVCHeadHeightKey->getCStringNoCopy(), 1024, 32);
		IOLog("IOProxyVideoHead  height=1024 (default)\n");
	}

	OSNumber *maxWidth = OSDynamicCast(OSNumber, dict->getObject(gIODVCHeadMaxWidthKey));
	if (!maxWidth) {
		setProperty(gIODVCHeadMaxWidthKey->getCStringNoCopy(), 1920, 32);
		IOLog("IOProxyVideoHead  maxWidth=1920 (default)\n");
	}
	
	OSNumber *maxHeight = OSDynamicCast(OSNumber, dict->getObject(gIODVCHeadMaxHeightKey));
	if (!maxHeight) {
		setProperty(gIODVCHeadMaxHeightKey->getCStringNoCopy(), 1200, 32);
		IOLog("IOProxyVideoHead  maxHeight=1200 (default)\n");
	}
	
	setProperty("compatible", "proxyVideoHead");								// a property for the framebuffer to match on
	
    return res;
}


void
com_doequalsglory_driver_IOProxyVideoHead::free(void) {
    IOLog("IOProxyVideoHead::free()\n");
    super::free();
}


IOService *
com_doequalsglory_driver_IOProxyVideoHead::probe(IOService *provider, SInt32
												 *score) {
    IOService *res = super::probe(provider, score);
    IOLog("IOProxyVideoHead::probe()\n");
    return res;
}


bool
com_doequalsglory_driver_IOProxyVideoHead::start(IOService *provider) {
    bool res = super::start(provider);
    IOLog("IOProxyVideoHead::start()\n");
	

	
	return res;
}


void
com_doequalsglory_driver_IOProxyVideoHead::stop(IOService *provider) {
    IOLog("IOProxyVideoHead::stop()\n");
	
	// free clients here?
	IOService *client = getClient();
	if (client) {
		IOLog("freeing client: %p", client);
	} else {
		IOLog("no client to free");
	}
	
    super::stop(provider);
}


#pragma mark -

/*! ----------------------------+---------------+-------------------------------
	@method		getResources()
	@abstract	<#brief description#>
	@discussion	what calls this?
	@param		<#name#> <#description#>
	@result		<#description#>
--------------------------------+---------------+---------------------------- */
IOReturn 
com_doequalsglory_driver_IOProxyVideoHead::getResources( void ) {
	IOLog("IOProxyVideoHead::getResources()\n");
	
    //return ((com_doequalsglory_driver_IOProxyVideoCard *)getProvider())->getNubResources( this );
	return kIOReturnSuccess;
}


// override setProperty, just pass straight to the framebuffer?

