/*
 *  IOProxyFramebuffer.cpp
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
/*
 * notes:
 *	this driver depends on IOProxyVideoCard, if not loading from /S/L/E,
 *	IOProxyVideoCard must be in the kextload path (-r path)
 *	
 *	this driver must be loaded at boot time, and cannot be inserted into a
 *	running system with kextload
 */

#define IOFRAMEBUFFER_PRIVATE
#include "IOProxyFramebuffer.h"
#undef IOFRAMEBUFFER_PRIVATE
#include "IOProxyVideoCard.h"
#include "EDID.h"

#include <IOKit/IOBufferMemoryDescriptor.h>
#include <libkern/OSBase.h>
#include <IOKit/IOLib.h>
#include <IOKit/IODeviceTreeSupport.h>


#define kIOPMIsPowerManagedKey		"IOPMIsPowerManaged"

#define kClassName					"IOProxyFramebuffer"
#define kDebugStr					kClassName ": "
//#define DEBUG_CALLS

#define kDepth8Bit		0
#define kDepth16Bit		1
#define kDepth32Bit		2

// why aren't these defined on the system somewhere? is it because they refer to
// the states we set up in enableController?
enum {
	kDFBSleepState = 0,
	kDFBDozeState = 1,
	kDFBWakeState = 2
};


const IODisplayModeInformation gProxyDisplayModes[] = {
	//	{   w,    h,  refresh, maxDepthIndex,               flags, reserved[4]}
	{   0,    0, 60 << 16, kDepth32Bit, kDisplayModeValidFlag},					// make things 1-based to fit in valid IODisplayModeID range 0x1..0x7fffffff
	
	// make sure the largest resolution is first, as we use that to specify
	// how much ram to allocate for the framebuffer
	
	{1920, 1200, 60 << 16, kDepth32Bit, kDisplayModeValidFlag},					// 23" cinema
	{1600, 1200, 60 << 16, kDepth32Bit, kDisplayModeValidFlag},
	{1440,  900, 60 << 16, kDepth32Bit, kDisplayModeValidFlag},					// mbp
	{1280, 1024, 60 << 16, kDepth32Bit, kDisplayModeValidFlag | kDisplayModeSafeFlag | kDisplayModeDefaultFlag},
	{1280,  854, 60 << 16, kDepth32Bit, kDisplayModeValidFlag},					// pb
	{1024,  768, 60 << 16, kDepth32Bit, kDisplayModeValidFlag},
	{   0,    0, 60 << 16, kDepth32Bit, kDisplayModeValidFlag | kDisplayModeAlwaysShowFlag}
};
const IOItemCount gProxyDisplayModeCount = sizeof(gProxyDisplayModes) / sizeof(IODisplayModeInformation) - 1;
const IOIndex kLargestDisplayMode = 1;
const IOIndex kStartupDisplayMode = 3;

#define super IOFramebuffer														// this convention makes it easy to invoke superclass methods

// cannot use the "super" macro with the OSDefineMetaClassAndStructors macro
OSDefineMetaClassAndStructors(com_doequalsglory_driver_IOProxyFramebuffer, IOFramebuffer)


#pragma mark setup and teardown
/*!
@method		start
	@abstract	entry point
	@discussion	paired with stop
	@param		<#name#> <#description#>
	@result		<#description#>
 */
bool
com_doequalsglory_driver_IOProxyFramebuffer::start(IOService *provider) {
#if defined(DEBUG_CALLS)
	IOLog("IOProxyFramebuffer::start(%s)\n", provider->getName());
#endif
	
    if(!super::start(provider)) {
        return false;
	}
	
	fProvider = OSDynamicCast(com_doequalsglory_driver_IOProxyVideoHead, provider);
    
    if (!fProvider) {
		return false;
	}
	
	setName("DEG,ProxyFramebuffer");

	const char *location = provider->getLocation();
	if (location) {
		setLocation(location);
	} else {
		return false;
	}
	
	OSBoolean *enabled = OSDynamicCast(OSBoolean, provider->getProperty("enabled"));
	if (enabled) {
		setProperty("enabled", enabled);
		IOLog("%s@%s->start() enabled: %d\n", getName(), getLocation(), enabled->getValue());
	} else {
		return false;
	}
	
	OSNumber *width = OSDynamicCast(OSNumber, provider->getProperty("width"));
	if (width) {
		setProperty("width", width);
		IOLog("%s@%s->start() width: %d\n", getName(), getLocation(), width->unsigned32BitValue());
	} else {
		return false;
	}
	
	OSNumber *height = OSDynamicCast(OSNumber, provider->getProperty("height"));
	if (height) {
		setProperty("height", height);
		IOLog("%s@%s->start() height: %d\n", getName(), getLocation(), height->unsigned32BitValue());
	} else {
		return false;
	}
	
	OSNumber *maxWidth = OSDynamicCast(OSNumber, provider->getProperty("maxWidth"));	// move to OSSymbol impl.
	if (maxWidth) {
		setProperty("maxWidth", maxWidth);
		IOLog("%s@%s->start() maxWidth: %d\n", getName(), getLocation(), maxWidth->unsigned32BitValue());
	} else {
		return false;
	}
	
	OSNumber *maxHeight = OSDynamicCast(OSNumber, provider->getProperty("maxHeight"));
	if (maxHeight) {
		setProperty("maxHeight", maxHeight);
		IOLog("%s@%s->start() maxHeight: %d\n", getName(), getLocation(), maxHeight->unsigned32BitValue());
	} else {
		return false;
	}
	
	// set EDID property here?
	
	fCurrentDisplayMode = 0;
	fCurrentDepth = 0;
	
	fBuffer = NULL;
	
	// call open on the provider/nub? it already appears to be retaining us
	//provider->open(this);
	
    return true;
}


void
com_doequalsglory_driver_IOProxyFramebuffer::stop(IOService *provider) {
#if defined(DEBUG_CALLS)
	IOLog("%s@%s->stop(%s)\n", getName(), getLocation(), provider->getName());
#endif
	
	// call close on the provider/nub?
	//provider->close(this);
	
	if (fBuffer) {
		fBuffer->release();
		fBuffer = NULL;
	}
	
	fCurrentDepth = 0;
	fCurrentDisplayMode = 0;
	
    super::stop(provider);
}



void
com_doequalsglory_driver_IOProxyFramebuffer::handleConnectChange(IOFramebuffer *inst, void *delay) {
#if defined(DEBUG_CALLS)
	IOLog("%s@%s->handleConnectChange()\n", inst->getName(), inst->getLocation());
#endif
	
	//deliverFramebufferNotification(kIOFBNotifyDisplayModeWillChange);
}

/*!
	@method		enableController
	@abstract	Perform first time setup of the framebuffer. 
	@discussion	IOFramebuffer subclasses should perform their
				initialization of the hardware here. The
				IOService start() method is not called at a
				time appropriate for this initialization.
	@param		<#name#> <#description#>
	@result		<#description#>
*/
IOReturn
com_doequalsglory_driver_IOProxyFramebuffer::enableController(void) {
//#if defined(DEBUG_CALLS)
	IOLog("%s@%s->enableController()\n", getName(), getLocation());
//#endif

	// set up power management
	fPowerState = kDFBWakeState;
	
	IOPMPowerState powerStates [] = {
		{kIOPMPowerStateVersion1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },										// kDFBSleepState
		{kIOPMPowerStateVersion1, 0, 0, IOPMPowerOn, 0, 0, 0, 0, 0, 0, 0, 0 },								// kDFBDozeState
		{kIOPMPowerStateVersion1, IOPMDeviceUsable, IOPMPowerOn, IOPMPowerOn, 0, 0, 0, 0, 0, 0, 0, 0 }		// kDFBWakeState
    };

    registerPowerDriver(this, powerStates, 3);
    temporaryPowerClampOn();
    changePowerStateTo(kDFBDozeState);
	getProvider()->setProperty(kIOPMIsPowerManagedKey, true);
	
	// we only allocate memory at enableController time instead of at
	// setDisplayMode, so make sure we have enough to handle whatever sized
	// display modes we might want to switch to
	UInt32 bufferSize = getApertureSize(kLargestDisplayMode, kDepth32Bit);
	IOLog("attempting to allocate %d bytes\n", bufferSize);
	//fBuffer = IOBufferMemoryDescriptor::withCapacity(bufferSize, kIODirectionInOut);
	
	// I _think kIOMemoryKernelUserShared doesn't use the zalloc pool so we can
	// grab more memory here, otherwise we're limited to <18mb total
	fBuffer = IOBufferMemoryDescriptor::withOptions(kIODirectionInOut | kIOMemoryKernelUserShared,
													bufferSize, page_size);
	if (!fBuffer) {
		IOLog("error allocating memory: %d bytes\n", bufferSize);
	}
	
	// set up initial display mode. should setDisplayMode do the
	// framebuffer memory allocation? no, because the framebuffer shouldn't
	// change size when the display mode changes
	IODisplayModeID initialDisplayMode;
	IOIndex initialDepth;
	getStartupDisplayMode(&initialDisplayMode, &initialDepth);
	setDisplayMode(initialDisplayMode, initialDepth);

	
	// register connect interrupt
	void *connectInterrupt;
	IOReturn err = kIOReturnSuccess;
	err = registerForInterruptType(kIOFBConnectInterruptType,
								   (IOFBInterruptProc) &handleConnectChange,
								   this,
								   NULL,
								   &connectInterrupt);
	
	return kIOReturnSuccess;
}


#pragma mark messaging

IOReturn
com_doequalsglory_driver_IOProxyFramebuffer::message(UInt32 type,
													 IOService *provider,
													 void *argument) {
	
    switch (type) {
		case kIOFBConnectInterruptType:
			IOLog("%s@%s->message(kIOFBConnectInterruptType)\n", getName(), getLocation());

			// generate a kIOFBConnectInterruptType interrupt somehow? this
			// calls the handler directly
			connectChangeInterrupt(this, 0);
			//checkConnectionChange(false);
			//deliverFramebufferNotification(kIOFBNotifyDisplayModeWillChange);
			break;
			
 		default:
			IOLog("%s@%s->message(%d)\n", getName(), getLocation(), type);
			break;
    }
	
	return kIOReturnSuccess;
}

#pragma mark memory management

/*! ----------------------------+---------------+-------------------------------
	@function	getApertureSize
	@abstract	<#brief description#>
	@discussion	<#comprehensive description#>
	@param		<#name#> <#description#>
	@result		<#description#>
--------------------------------+---------------+---------------------------- */
UInt32 
com_doequalsglory_driver_IOProxyFramebuffer::getApertureSize(IODisplayModeID displayMode, IOIndex depth) {
//#if defined(DEBUG_CALLS)
	IOLog("%s@%s->getApertureSize(displayMode: %d, depth: %d)\n", getName(), getLocation(), displayMode, depth);
//#endif

	IOPixelInformation info;
	getPixelInformation(displayMode, depth, kIOFBSystemAperture, &info);

	return (info.bytesPerRow * info.activeHeight) + 128;
}


/*! @function	getApertureRange
    @abstract	Return reference to IODeviceMemory object representing
				memory range of framebuffer.
    @discussion	IOFramebuffer subclasses must implement this method to
				describe the memory used by the framebuffer in the current
				mode. The OS will map this memory range into user space
				for client access - the range should only include vram
				memory not hardware registers.
    @param		aperture The system will only access the aperture kIOFBSystemAperture.
    @result		an IODeviceMemory instance. A reference will be consumed
				by the caller for each call of this method - the implementatation
				should create a new instance of IODeviceMemory for each
				call, or return one instance with a retain for each call.
*/
IODeviceMemory *
com_doequalsglory_driver_IOProxyFramebuffer::getApertureRange(IOPixelAperture aper) {
#if defined(DEBUG_CALLS)
	IOLog("%s@%s->getApertureRange(%p)\n", getName(), getLocation(), aper);
#endif

	if (fCurrentDisplayMode == 0) {
		return NULL;
	}

	if (aper != kIOFBSystemAperture) {
		return NULL;
	}
	
	if (fBuffer == NULL) {
		return NULL;
	}
		
	IODeviceMemory *apertureRange = IODeviceMemory::withRange(fBuffer->getPhysicalAddress(),
															  getApertureSize(fCurrentDisplayMode, fCurrentDepth));
		
	return apertureRange;
}

/*!
	@method		getVRAMRange
	@abstract	Return reference to IODeviceMemory object representing memory
					range of all the cards vram.
	@discussion	IOFramebuffer subclasses should implement this method to describe
					all the vram memory available on the card. The OS will map
					this memory range into user space for client access - the
					range should only include vram memory not hardware registers.
	@result		an IODeviceMemory instance. A reference will be consumed by the
					caller for each call of this method - the implementatation
					should create a new instance of IODeviceMemory for each call,
					or return one instance with a retain for each call.
*/
IODeviceMemory * 
com_doequalsglory_driver_IOProxyFramebuffer::getVRAMRange(void) {
	if (fBuffer == NULL) {
		return NULL;
	}
	
	fBuffer->retain();
	
#if defined(DEBUG_CALLS)
	IOLog("%s@%s->getVRAMRange() = %p\n", getName(), getLocation(), fBuffer);
#endif

	return (IODeviceMemory *)fBuffer;
}

#pragma mark framebuffer info

/*! @function getPixelFormats
    @abstract	List the pixel formats the framebuffer supports.
    @discussion	IOFramebuffer subclasses must implement this method to
				return an array of strings representing the possible
				pixel formats available in the framebuffer.
    @result		A const char * pointer. The string consists of a concatenation
				of each pixel format string separated by the NULL character.
				The commonly supported pixel formats for Mac OS X are
				defined as IO8BitIndexedPixels, IO16BitDirectPixels,
				IO32BitDirectPixels.
*/
const char *
com_doequalsglory_driver_IOProxyFramebuffer::getPixelFormats() {
	static const char *pixelFormats = IO8BitIndexedPixels "\0" IO16BitDirectPixels "\0" IO32BitDirectPixels "\0\0";
	//static const char *pixelFormats = IO32BitDirectPixels "\0\0";
	
#if defined(DEBUG_CALLS)
	IOLog("%s@%s->getPixelFormats() = %s\n", getName(), getLocation(), pixelFormats);
#endif

    return pixelFormats;
}

/*! @function	getInformationForDisplayMode
    @abstract	Return information about a given display mode.
    @discussion IOFramebuffer subclasses must implement this method to
				return information in the IODisplayModeInformation
				structure for the display mode with the passed ID. 
    @param		displayMode A display mode ID previously returned by
				getDisplayModes().
    @param		info Pointer to a structure of type IODisplayModeInformation
				to be filled out by the driver. IODisplayModeInformation
				is documented in IOGraphicsTypes.h.
    @result		an IOReturn code. A return other than kIOReturnSuccess will
				prevent the system from using the device.
*/
IOReturn
com_doequalsglory_driver_IOProxyFramebuffer::getInformationForDisplayMode(IODisplayModeID displayMode, IODisplayModeInformation *info) {
#if defined(DEBUG_CALLS)
	IOLog("%s@%s->getInformationForDisplayMode(%d)\n", getName(), getLocation(), displayMode);
#endif
	
	if (!info) {
		return kIOReturnBadArgument;
	}
	bzero (info, sizeof (*info));
	
	info->maxDepthIndex	= kDepth32Bit;
	info->nominalWidth = gProxyDisplayModes[displayMode].nominalWidth;
	info->nominalHeight	= gProxyDisplayModes[displayMode].nominalHeight;
	//info->refreshRate = 0x3C0000;												// 60Hz in fixed point 16.16.
	//info->refreshRate	= 60 << 16;												// from IOBootFramebuffer.cpp
	//info->refreshRate = 0x0;
	info->refreshRate = gProxyDisplayModes[displayMode].refreshRate;
	info->flags = gProxyDisplayModes[displayMode].flags;
	
    return kIOReturnSuccess;
}

/*! @function	getPixelFormatsForDisplayMode
    @abstract	Obsolete.
    @discussion IOFramebuffer subclasses must implement this method to return zero. 
    @param		displayMode Ignored.
    @param		depth Ignored.
    @result		Return zero.
*/
UInt64
com_doequalsglory_driver_IOProxyFramebuffer::getPixelFormatsForDisplayMode(IODisplayModeID, IOIndex) {
	return 0;
}

/*! @function	getPixelInformation
    @abstract	Return information about the framebuffer format for a
				given display mode and depth.
    @discussion IOFramebuffer subclasses must implement this method to
				return information in the IOPixelInformation structure
				for the display mode with the passed ID, depth index
				and aperture. The aperture utilized by the system is
				always kIOFBSystemAperture. Drivers may define alternative
				apertures, being a view of the framebuffer in a different
				pixel format from the default.
    @param		displayMode A display mode ID previously returned by
				getDisplayModes().
    @param		depth An index from zero to the value of the maxDepthIndex
				field from the IODisplayModeInformation structure (inclusive).
    @param		info Pointer to a structure of type IOPixelInformation to
				be filled out by the driver. IOPixelInformation is
				documented in IOGraphicsTypes.h.
    @result		an IOReturn code. A return other than kIOReturnSuccess
				will prevent the system from using the device.
*/
IOReturn
com_doequalsglory_driver_IOProxyFramebuffer::getPixelInformation(IODisplayModeID displayMode, IOIndex depth, IOPixelAperture aperture, IOPixelInformation *info) {	
#if defined(DEBUG_CALLS)
	IOLog("%s@%s->getPixelInformation(displayMode: %d, depth: %d)\n", getName(), getLocation(), displayMode, depth);
#endif
	
	if (info == NULL) {
		return kIOReturnBadArgument;
	}
	
    bzero (info, sizeof (*info));

	info->activeWidth = gProxyDisplayModes[displayMode].nominalWidth;
	info->activeHeight = gProxyDisplayModes[displayMode].nominalHeight;
	
	switch(depth) {
		case kDepth8Bit:
			strcpy(info->pixelFormat, IO8BitIndexedPixels);
			info->pixelType = kIOCLUTPixels;
			info->componentMasks[0] = 0xff;
			info->bitsPerPixel = 8;
			info->componentCount = 1;
			info->bitsPerComponent = 8;
			break;
			
		case kDepth16Bit:
			strcpy(info->pixelFormat, IO16BitDirectPixels);
			info->pixelType = kIORGBDirectPixels;
			info->componentMasks[0] = 0x7c00;
			info->componentMasks[1] = 0x03e0;
			info->componentMasks[2] = 0x001f;
			info->bitsPerPixel = 16;
			info->componentCount = 3;
			info->bitsPerComponent = 5;
			break;
			
		case kDepth32Bit:
		default:
			strcpy (info->pixelFormat, IO32BitDirectPixels);
			info->pixelType = kIORGBDirectPixels;
			info->componentMasks[0] = 0x00FF0000;
			info->componentMasks[1] = 0x0000FF00;
			info->componentMasks[2] = 0x000000FF;
			info->bitsPerPixel = 32;
			info->componentCount = 3;
			info->bitsPerComponent = 8;
	}
	
	info->bytesPerRow = (info->activeWidth * info->bitsPerPixel / 8) + 32;		// 32 byte row header?

    return kIOReturnSuccess;
}

bool
com_doequalsglory_driver_IOProxyFramebuffer::isConsoleDevice(void) {
	bool isConsoleDevice = getProvider()->getProperty("AAPL,boot-display") != NULL;
	
#if defined(DEBUG_CALLS)
	IOLog("%s@%s->isConsoleDevice() = %d\n", getName(), getLocation(), isConsoleDevice);
#endif

	return isConsoleDevice;
}

/*! ----------------------------+---------------+-------------------------------
	@function	getTimingInfoForDisplayMode
	@abstract	according to IOFramebuffer.h, I don't need to implement this if
				I'm not supporting detailed timing modes.
	@discussion	<#comprehensive description#>
	@param		<#name#> <#description#>
	@result		<#description#>
--------------------------------+---------------+---------------------------- */
//IOReturn
//com_doequalsglory_driver_IOProxyFramebuffer::getTimingInfoForDisplayMode(IODisplayModeID displayMode, IOTimingInformation *info) {
//#if defined(DEBUG_CALLS)
//	IOLog(kDebugStr "getTimingInfoForDisplayMode(displayMode: %d)\n", displayMode);
//#endif
//
//	if (info == NULL) {
//		return kIOReturnBadArgument;
//	}
//	
//	//switch (displayMode) {
//		//case kDisplayMode:
//	//	default:
////			info->appleTimingID = gProxyDisplayModes[displayMode].appleTimingID;
//	//info->appleTimingID = kIOTimingIDApple_FixedRateLCD;
//	//info->appleTimingID = kIOTimingIDVESA_1280x1024_60hz;
//	info->appleTimingID = kIOTimingIDInvalid;
//	info->flags = 0;
//	//		break;
//	//}
//
//	return kIOReturnSuccess;
//}

IOReturn
com_doequalsglory_driver_IOProxyFramebuffer::getAttribute(IOSelect attribute, UInt32 *value) {
	if (value == NULL) {
		return kIOReturnBadArgument;
	}
	
	IOReturn ret;
	
	switch (attribute) {
		case kIOHardwareCursorAttribute:										// 'crsr'
			*value = 0;
			ret = kIOReturnSuccess;
			break;
		
		case kIOPowerAttribute:													// 'powr',
		
		case kIOMirrorAttribute:												// 'mirr',
		case kIOMirrorDefaultAttribute:											// 'mrdf',

		case kIOCapturedAttribute:												// 'capd',

		case kIOCursorControlAttribute:											// 'crsc',

		case kIOSystemPowerAttribute:											// 'spwr',
		case kIOVRAMSaveAttribute:												// 'vrsv',
		case kIODeferCLUTSetAttribute:											// 'vclt'

		default:
			ret = kIOReturnUnsupported;
			break;
	}
	
#if defined(DEBUG_CALLS)
	if (ret == kIOReturnSuccess) {
		IOLog("%s@%s->getAttribute(attribute: %.4s, *outValue: %u) = %d (kIOReturnSuccess)\n", getName(), getLocation(), &attribute, *value, ret);
	} else if (ret == kIOReturnUnsupported) {
		IOLog("%s@%s->getAttribute(attribute: %.4s, *outValue: %u) = %d (kIOReturnUnsupported)\n", getName(), getLocation(), &attribute, *value, ret);
	} else {
		IOLog("%s@%s->getAttribute(attribute: %.4s, *outValue: %u) = %d\n", getName(), getLocation(), &attribute, *value, ret);
	}
#endif

	return kIOReturnSuccess;
}

IOReturn
com_doequalsglory_driver_IOProxyFramebuffer::setAttribute(IOSelect attribute, UInt32 value) {
#if defined(DEBUG_CALLS)
	IOLog("%s@%s->setAttribute(attribute: %.4s, value: %u)\n", getName(), getLocation(), &attribute, value);
#endif

	switch (attribute) {
		case kIOPowerAttribute:
			handleEvent((value >= kDFBWakeState) ? kIOFBNotifyWillPowerOn : kIOFBNotifyWillPowerOff);
			handleEvent((value >= kDFBWakeState) ? kIOFBNotifyDidPowerOn : kIOFBNotifyDidPowerOff);
			
			fPowerState = value;
			return kIOReturnSuccess;
			
		default:
			return super::setAttribute(attribute, value);
	}
}


#pragma mark connection info

/*! ----------------------------+---------------+-------------------------------
	@function	getConnectionCount()
	@abstract	number of physical monitors plugged in
	@discussion	multi-head systems should have one IOFramebuffer per head, not
				one IOFramebuffer spanning multiple heads
				
				setting this to 0 doesn't seem to disable the framebuffer
	@param		<#name#> <#description#>
	@result		<#description#>
--------------------------------+---------------+---------------------------- */
IOItemCount
com_doequalsglory_driver_IOProxyFramebuffer::getConnectionCount(void) {
	
	OSBoolean *enabled = OSDynamicCast(OSBoolean, getProvider()->getProperty("enabled"));
	
	bool ret = enabled->getValue();
	
//#if defined(DEBUG_CALLS)
	IOLog("%s@%s->getConnectionCount() = %d\n", getName(), getLocation(), ret);
//#endif
	
	return ret;
}


IOReturn
com_doequalsglory_driver_IOProxyFramebuffer::getAttributeForConnection(IOIndex connectIndex, IOSelect attribute, UInt32 *value) {	
	if (value == NULL) {
		return kIOReturnBadArgument;
	}
	
	IOReturn ret;
	
	switch (attribute) {
		case kConnectionEnable:						 							// 'enab'
			*value = 1;
			ret = kIOReturnSuccess;
		
		case kConnectionSupportsHLDDCSense:										// 'hddc'
			// If the framebuffer supports the DDC methods hasDDCConnect() and
			// getDDCBlock() it should return success (and no value) for this attribute.
			*value = 0;
			ret = kIOReturnSuccess;
		
		case kConnectionFlags:													// 'flgs'
		case kConnectionSupportsAppleSense:										// 'asns'
		case kConnectionSupportsLLDDCSense:										// 'lddc'
		case kConnectionPower:													// 'powr'
		case kConnectionPostWake:												// 'pwak'
		case kConnectionDisplayParameterCount:									// 'pcnt'
		case kConnectionDisplayParameters:										// 'parm'
		default:
			ret = kIOReturnUnsupported;
	}
	
#if defined(DEBUG_CALLS)
	if (ret == kIOReturnSuccess) {
		IOLog("%s@%s->getAttributeForConnection(connectIndex: %d, attribute: %.4s, *outValue: %u) = %d (kIOReturnSuccess)\n", getName(), getLocation(), connectIndex, &attribute, *value, ret);
	} else if (ret == kIOReturnUnsupported) {
		IOLog("%s@%s->getAttributeForConnection(connectIndex: %d, attribute: %.4s, *outValue: %u) = %d (kIOReturnUnsupported)\n", getName(), getLocation(), connectIndex, &attribute, *value, ret);
	} else {
		IOLog("%s@%s->getAttributeForConnection(connectIndex: %d, attribute: %.4s, *outValue: %u) = %d\n", getName(), getLocation(), connectIndex, &attribute, *value, ret);
	}
#endif

	return ret;
}

IOReturn
com_doequalsglory_driver_IOProxyFramebuffer::setAttributeForConnection(IOIndex connectIndex, IOSelect attribute, UInt32 value) {
#if defined(DEBUG_CALLS)
	IOLog("%s@%s->setAttributeForConnection(connectIndex: %d, attribute: %.4s, value: %u)\n", getName(), getLocation(), connectIndex, &attribute, value);
#endif

	switch (attribute) {
		case kConnectionPower:
			return kIOReturnSuccess;
		
		default:
			return kIOReturnUnsupported;
	}
}


/*! ----------------------------+---------------+-------------------------------
	@function	hasDDCConnect
	@abstract	support DDC for display name
	@discussion	<#comprehensive description#>
	@param		<#name#> <#description#>
	@result		<#description#>
--------------------------------+---------------+---------------------------- */
bool
com_doequalsglory_driver_IOProxyFramebuffer::hasDDCConnect(IOIndex connectIndex) {
#if defined(DEBUG_CALLS)
	IOLog("%s@%s->hasDDCConnect(%d)\n", getName(), getLocation(), connectIndex);
#endif

	return true;
}

IOReturn
com_doequalsglory_driver_IOProxyFramebuffer::getDDCBlock(IOIndex connectIndex, UInt32 blockNumber, IOSelect blockType,
												IOOptionBits options, UInt8 *data, IOByteCount *length) {
#if defined(DEBUG_CALLS)
	IOLog("%s@%s->getDDCBlock(connectIndex: %d, blockNumber: %u, blockType: %u, options: %u)\n", getName(), getLocation(), connectIndex, blockNumber, blockType, options);
#endif

	if (data == NULL) {
		return kIOReturnBadArgument;
	}
	
	EDID edid;
	
	bzero(&edid, sizeof(edid));
	
	edid.descriptorBlock1.blockType = 0xfc;										// monitor name
	edid.descriptorBlock1.info[0] = 'n';
	edid.descriptorBlock1.info[1] = 'e';
	edid.descriptorBlock1.info[2] = 'd';
	
	// copy edid out
	memcpy(data, &edid, sizeof(edid));
	*length = sizeof(edid);
	
	return kIOReturnSuccess;
}


#pragma mark display mode accessors
/*! @function	getDisplayModeCount
    @abstract	Return the number of display modes the framebuffer supports.
    @discussion IOFramebuffer subclasses must implement this method to
				return a count of the display modes available. This count
				should change unless a connection change is posted for the
				device indicated the framebuffer and/or display
				configuration has changed.
				
				think this is wrong - the count SHOULDN'T change unless a..

				if disabled, return 0 here?

    @result		A count of the display modes available.
*/
IOItemCount
com_doequalsglory_driver_IOProxyFramebuffer::getDisplayModeCount() {
	IOItemCount displayModeCount = gProxyDisplayModeCount;
	
//#if defined(DEBUG_CALLS)
	IOLog("%s@%s->getDisplayModeCount() = %u\n", getName(), getLocation(), displayModeCount);
//#endif

	//OSBoolean *enabled;
	
	return displayModeCount;
}


/*! @function	getDisplayModes
    @abstract	Return the number of display modes the framebuffer supports.
    @discussion IOFramebuffer subclasses must implement this method to
				return an array of display mode IDs available for the
				framebuffer. The IDs are defined by the driver in the
				range 0x00000001 - 0x7fffffff, and should be constant for
				a given display mode. 
    @param		allDisplayModes A caller allocated buffer with the size
				given by the result of getDisplayModeCount().
    @result		an IOReturn code. A return other than kIOReturnSuccess
				will prevent the system from using the device.
*/
IOReturn
com_doequalsglory_driver_IOProxyFramebuffer::getDisplayModes(IODisplayModeID *allDisplayModes) {
//#if defined(DEBUG_CALLS)
	IOLog("%s@%s->getDisplayModes(*outAllDisplayModes: %p)\n", getName(), getLocation(), allDisplayModes);
//#endif
	if (allDisplayModes == NULL) {
		return kIOReturnBadArgument;
	}
	
	for (UInt32 i = 1; i <= gProxyDisplayModeCount; i++) {
		*allDisplayModes = i;
		allDisplayModes++;
	}
	
	return kIOReturnSuccess;
}


/*!
	@function	setCurrentDisplayMode
	@abstract	Set the framebuffers current display mode and depth.
	@discussion IOFramebuffer subclasses should implement this method to set
				the current mode and depth. Other than at enableController()
				time, this is the only method that should change the framebuffer
				format and is synchronized with clients and attached
				accelerators to make sure access to the device is disallowed
				during the change.
 
				!! we are not allowed to change the VRAM here whatsoever !!
 
	@param		displayMode A display mode ID representing the new mode.
	@param		depth An index indicating the new depth configuration of the
				framebuffer. The index should range from zero to the value of
				the maxDepthIndex field from the IODisplayModeInformation
				structure for the display mode.
	@result		an IOReturn code. A return other than kIOReturnSuccess will
				prevent the system from using the device.
*/
IOReturn 
com_doequalsglory_driver_IOProxyFramebuffer::setDisplayMode(IODisplayModeID displayMode, IOIndex depth) {
//#if defined(DEBUG_CALLS)
	IOLog("%s@%s->setDisplayMode(displayMode: %d, depth: %d)\n", getName(), getLocation(), displayMode, depth);
//#endif

	if ((displayMode == fCurrentDisplayMode) && (depth == fCurrentDepth)) {
		return kIOReturnSuccess;
	}
	
	fCurrentDisplayMode = displayMode;
	fCurrentDepth = depth;
	
	return kIOReturnSuccess;
}

/*! @function	getCurrentDisplayMode
    @abstract	Return the framebuffers current display mode and depth.
    @discussion	IOFramebuffer subclasses must implement this method to
				return the current mode and depth.
    @param		displayMode A display mode ID representing the current mode.
    @param		depth An index indicating the depth configuration of the
				framebuffer. The index should range from zero to the value
				of the maxDepthIndex field from the IODisplayModeInformation
				structure for the display mode.
    @result		an IOReturn code. A return other than kIOReturnSuccess will
				prevent the system from using the device.
*/
IOReturn
com_doequalsglory_driver_IOProxyFramebuffer::getCurrentDisplayMode(IODisplayModeID *displayMode, IOIndex *depth) {
	if (displayMode == NULL) {
		return kIOReturnBadArgument;
	}
	
	*displayMode = fCurrentDisplayMode;
	
	if (depth == NULL) {
		return kIOReturnBadArgument;
	}
	
	*depth = fCurrentDepth;
	
//#if defined(DEBUG_CALLS)
	IOLog("%s@%s->getCurrentDisplayMode(*outDisplayMode: %d, *outDepth: %d)\n", getName(), getLocation(), *displayMode, *depth);
//#endif

	return kIOReturnSuccess;
}

IOReturn 
com_doequalsglory_driver_IOProxyFramebuffer::setStartupDisplayMode(IODisplayModeID displayMode, IOIndex depth) {
#if defined(DEBUG_CALLS)
	IOLog("%s@%s->setStartupDisplayMode(displayMode: %d, depth %d)\n", getName(), getLocation(), displayMode, depth);
#endif

	return kIOReturnUnsupported;
}

IOReturn 
com_doequalsglory_driver_IOProxyFramebuffer::getStartupDisplayMode(IODisplayModeID *displayMode, IOIndex *depth) {	
	if (displayMode == NULL) {
		return kIOReturnBadArgument;
	}
	*displayMode = kStartupDisplayMode;
	
	if (depth == NULL) {
		return kIOReturnBadArgument;
	}
	*depth = kDepth32Bit;
	
#if defined(DEBUG_CALLS)
	IOLog("%s@%s->getStartupDisplayMode(*outDisplayMode: %d, *outDepth: %d)\n", getName(), getLocation(), *displayMode, *depth);
#endif

	return kIOReturnSuccess;
}