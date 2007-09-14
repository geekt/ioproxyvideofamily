/*
 *  IOProxyVideoCardUserClient.cpp
 *  IOProxyVideoCard
 *
 *  Created by thrust on 061213.
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

#include "IOProxyVideoCardUserClient.h"
#include "IOProxyVideoCard.h"

#include <IOKit/IOLib.h>


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
#define super IOUserClient
OSDefineMetaClassAndStructors(com_doequalsglory_driver_IOProxyVideoCardUserClient, IOUserClient)
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */


IOExternalMethod *
com_doequalsglory_driver_IOProxyVideoCardUserClient::getTargetAndMethodForIndex(IOService ** target, UInt32 index) {
    IOLog("IOProxyVideoCardUserClient::getTargetAndMethodForIndex(index = %d)\n", (int)index);
    
    static const IOExternalMethod sMethods[kNumberOfMethods] = 
    {
        {   // kMyUserClientOpen
            NULL,																// the IOService * will be determined at runtime below
            (IOMethod) &com_doequalsglory_driver_IOProxyVideoCardUserClient::open,	 // Method pointer.
            kIOUCScalarIScalarO,												// Scalar Input, Scalar Output.
            0,																	// # scalar input values
            0																	// # scalar output values
        },
        {   // kMyUserClientClose
            NULL,																// the IOService * will be determined at runtime below
            (IOMethod) &com_doequalsglory_driver_IOProxyVideoCardUserClient::close, // Method pointer.
            kIOUCScalarIScalarO,												// Scalar Input, Scalar Output.
            0,																	// # scalar input values
            0																	// # scalar output values
        },
		{	// kConnectionCountMethod
			NULL,																// the IOService * will be determined at runtime below
			(IOMethod) &com_doequalsglory_driver_IOProxyVideoCard::connectionCount,	// Method pointer.
			kIOUCScalarIScalarO,												// Scalar Input, Scalar Output.
			0,																	// # scalar input values
			1																	// # scalar output values
		},
		{	// kConnectionPropertiesMethod
			NULL,																// the IOService * will be determined at runtime below
			(IOMethod) &com_doequalsglory_driver_IOProxyVideoCard::connectionProperties,	// method pointer
			kIOUCScalarIStructO,												// scalar input, struct output
			1,																	// # scalar input values
			sizeof(ConnectionStruct)											// size of the output struct
		},
		{	// kSetConnectionPropertiesMethod
			NULL,
			(IOMethod) &com_doequalsglory_driver_IOProxyVideoCard::setConnectionProperties,	// method pointer
			kIOUCScalarIStructI,												// scalar input, struct input
			1,																	// # scalar input values
			sizeof(ConnectionStruct)											// size of the input struct
		},
		{	// kApplyChangesMethod
			NULL,																// the IOService * will be determined at runtime below
			(IOMethod) &com_doequalsglory_driver_IOProxyVideoCard::applyChanges,	// method pointer
			kIOUCScalarIScalarO,												// scalar input, scalar output
			0,																	// # scalar input values
			0																	// # scalar output values
		},
    };
    
    // Make sure that the index of the function we're calling actually exists in the function table.
    if (index < (UInt32)kNumberOfMethods) {
        if (index == kMyUserClientOpen || index == kMyUserClientClose) {
            // these methods exist in IOProxyVideoCardUserClient, so return
			// a pointer to com_doequalsglory_driver_IOProxyVideoCardUserClient
			IOLog("IOProxyVideoCardUserClient  returning pointer to self\n");
            *target = this;
			
        } else {
            // these methods exist in IOProxyVideoCard, so return a pointer
			// to com_doequalsglory_driver_IOProxyVideoCard
			IOLog("IOProxyVideoCardUserClient  returning pointer to provider\n");
            *target = fProvider;
        }
        
        return (IOExternalMethod *) &sMethods[index];
    }

    return NULL;
}


bool
com_doequalsglory_driver_IOProxyVideoCardUserClient::initWithTask(task_t owningTask, void *security_id, UInt32 type) {
    IOLog("IOProxyVideoCardUserClient::initWithTask()\n");
    
    if (!super::initWithTask(owningTask, security_id , type)) {
        return false;
    }
	
    if (!owningTask) {
		return false;
	}
	
    fTask = owningTask;
    fProvider = NULL;
    fDead = false;
        
    return true;
}


#pragma mark -

bool 
com_doequalsglory_driver_IOProxyVideoCardUserClient::start(IOService * provider) {
    IOLog("IOProxyVideoCardUserClient::start()\n");
    
    if(!super::start(provider)) {
        return false;
	}
	
    fProvider = OSDynamicCast(com_doequalsglory_driver_IOProxyVideoCard, provider);
    
    if (!fProvider) {
		return false;
	}
	
	setName("ProxyVideoCardUC");
	
    return true;
}


void 
com_doequalsglory_driver_IOProxyVideoCardUserClient::stop(IOService * provider) {
    IOLog("IOProxyVideoCardUserClient::stop()\n");
    
    super::stop(provider);
}


IOReturn 
com_doequalsglory_driver_IOProxyVideoCardUserClient::open(void) {
    IOLog("IOProxyVideoCardUserClient::open()\n");
    
    // If we don't have an fProvider, this user client isn't going to do much, so return kIOReturnNotAttached.
    if (!fProvider) {
        return kIOReturnNotAttached;
	}
	
    // Call fProvider->open, and if it fails, it means someone else has already opened the device.
    if (!fProvider->open(this)) {
		return kIOReturnExclusiveAccess;
	}
	
    return kIOReturnSuccess;
}


IOReturn 
com_doequalsglory_driver_IOProxyVideoCardUserClient::close(void) {
    IOLog("IOProxyVideoCardUserClient::close()\n");
            
    // If we don't have an fProvider, then we can't really call the fProvider's close() function, so just return.
    if (!fProvider) {
        return kIOReturnNotAttached;
	}
	
    // Make sure the fProvider is open before we tell it to close.
    if (fProvider->isOpen(this)) {
        fProvider->close(this);
    }
	
    return kIOReturnSuccess;
}


#pragma mark -

IOReturn 
com_doequalsglory_driver_IOProxyVideoCardUserClient::clientClose(void) {
    IOLog("IOProxyVideoCardUserClient::clientClose()\n");
    
    // release my hold on my parent (if I have one).
    close();
    
    if (fTask) {
		fTask = NULL;
    }
	
    fProvider = NULL;
    terminate();
    
    // DONT call super::clientClose, which just returns notSupported
    
    return kIOReturnSuccess;
}


IOReturn 
com_doequalsglory_driver_IOProxyVideoCardUserClient::clientDied(void) {
    IOReturn ret = kIOReturnSuccess;
    IOLog("IOProxyVideoCardUserClient::clientDied()\n");

    fDead = true;
    
    // this just calls clientClose
    ret = super::clientDied();

    return ret;
}


IOReturn
com_doequalsglory_driver_IOProxyVideoCardUserClient::message(UInt32 type, IOService * provider,  void * argument) {
    IOLog("IOProxyVideoCardUserClient::message()\n");
    
    switch ( type )
    {
        default:
            break;
    }
    
    return super::message(type, provider, argument);
}


bool 
com_doequalsglory_driver_IOProxyVideoCardUserClient::finalize(IOOptionBits options) {
    bool ret;
    IOLog("IOProxyVideoCardUserClient::finalize()\n");
    
    ret = super::finalize(options);
    
    return ret;
}


bool 
com_doequalsglory_driver_IOProxyVideoCardUserClient::terminate(IOOptionBits options) {
    bool ret;
    IOLog("IOProxyVideoCardUserClient::terminate()\n");

    ret = super::terminate(options);
    
    return ret;
}