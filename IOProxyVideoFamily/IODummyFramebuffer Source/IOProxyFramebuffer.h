/*
 *  IOProxyFramebuffer.h
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

#ifndef __IOPROXYFRAMEBUFFER_H__
#define __IOPROXYFRAMEBUFFER_H__

#include <IOKit/graphics/IOFramebuffer.h>

class IOBufferMemoryDescriptor;


// if this is an IOFramebuffer instead of an IOService, free fails and
// we can't unload. why is that? probably because the windowserver maintains
// a reference and doesn't let go
class com_doequalsglory_driver_IOProxyFramebuffer : public IOFramebuffer {
	
	OSDeclareDefaultStructors(com_doequalsglory_driver_IOProxyFramebuffer)
	
public:
	// generic kext setup & teardown
	//virtual bool				init(OSDictionary *properties = 0);
	//virtual bool				attach(IOService *provider);
	//virtual IOService *		probe(IOService *provider, SInt32 *score);
	//virtual void				detach(IOService *provider);
	//virtual void				free();
	
	virtual bool				start(IOService *provider);
	virtual void				stop(IOService *provider);
	
	//virtual bool				requestTerminate( IOService * provider, IOOptionBits options );
	
	virtual IOReturn			open(void);
	virtual void				close(void);									// close is not being called
	
	//
	// IOFramebuffer specific methods
	//
	
	// perform first-time setup of the framebuffer
	virtual IOReturn			enableController(void);
	
	// "user client" interface
	IOReturn					message(UInt32 type, IOService *provider, void *argument);
	static void					handleConnectChange(IOFramebuffer *inst, void *delay);
	
	// memory management
	virtual UInt32				getApertureSize(IODisplayModeID, IOIndex);
	virtual IODeviceMemory *	getApertureRange(IOPixelAperture);
	virtual IODeviceMemory *	getVRAMRange(void);
	
	// framebuffer info
	virtual const char *		getPixelFormats();
	virtual IOReturn			getInformationForDisplayMode(IODisplayModeID, IODisplayModeInformation *);
	virtual UInt64				getPixelFormatsForDisplayMode(IODisplayModeID, IOIndex);
	virtual IOReturn			getPixelInformation(IODisplayModeID, IOIndex, IOPixelAperture, IOPixelInformation *);
	
	virtual bool				isConsoleDevice(void);
	
	virtual IOReturn			getTimingInfoForDisplayMode(IODisplayModeID, IOTimingInformation *);
	
	virtual IOReturn			getAttribute(IOSelect, UInt32 *);
	virtual IOReturn			setAttribute(IOSelect, UInt32);
	
	// connection info
	virtual IOItemCount			getConnectionCount(void);
	
	virtual IOReturn			getAttributeForConnection(IOIndex, IOSelect, UInt32 *);
	virtual IOReturn			setAttributeForConnection(IOIndex, IOSelect, UInt32);
	
	virtual bool				hasDDCConnect(IOIndex);
	virtual IOReturn			getDDCBlock(IOIndex, UInt32, IOSelect, IOOptionBits, UInt8 *, IOByteCount *);
	
	// display mode accessors
	virtual IOItemCount			getDisplayModeCount();
	virtual IOReturn			getDisplayModes(IODisplayModeID *);
	
	virtual IOReturn			setDisplayMode(IODisplayModeID, IOIndex);
	virtual IOReturn			getCurrentDisplayMode(IODisplayModeID *, IOIndex *);
	
	virtual IOReturn			setStartupDisplayMode(IODisplayModeID, IOIndex);
    virtual IOReturn			getStartupDisplayMode(IODisplayModeID *, IOIndex *);
	
private:
	IOService *					fProvider;
	
	IOBufferMemoryDescriptor *	fBuffer;
	
	IODisplayModeID				fCurrentDisplayMode;
	IOIndex						fCurrentDepth;
	
	UInt32						fPowerState;

//	int							maxWidth,
//								maxHeight;

};

#endif