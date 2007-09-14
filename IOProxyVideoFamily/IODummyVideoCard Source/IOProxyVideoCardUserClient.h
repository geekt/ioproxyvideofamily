/*
 *  IOProxyVideoCardUserClient.h
 *  IOProxyVideoFamily
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

#include <IOKit/IOService.h>
#include <IOKit/IOUserClient.h>
#include "UserClientCommon.h"


class com_doequalsglory_driver_IOProxyVideoCardUserClient : public IOUserClient {
    OSDeclareDefaultStructors(com_doequalsglory_driver_IOProxyVideoCardUserClient)
    
protected:
    IOService					*fProvider;
    task_t						fTask;
    bool						fDead;
	
public:
		// IOUserClient methods
	virtual IOReturn			open(void);
    virtual IOReturn			close(void);
    
    virtual void				stop(IOService * provider);
    virtual bool				start(IOService * provider);
    
    virtual IOReturn			message(UInt32 type, IOService * provider,  void * argument = 0);
    
    virtual bool				initWithTask(task_t owningTask, void * security_id, UInt32 type);
    virtual bool				finalize(IOOptionBits options);
	
    virtual IOExternalMethod *	getTargetAndMethodForIndex(IOService ** target, UInt32 index);
    virtual IOReturn			clientClose(void);
    virtual IOReturn			clientDied(void);
    virtual bool				terminate(IOOptionBits options = 0);    
};