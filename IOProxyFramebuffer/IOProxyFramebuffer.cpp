// ioproxyfb
// 2006 ritchie argue
//
// based on OpenControlFramebuffer and VirtualFramebuffer by
// Ryan Rempel / Other World Computing
//
// consider making a proxy video card that exposes multiple nubs,
// one for each remote screen. then the proxyfb matches on these
// providing multiple framebuffers? how do dual-head card drivers work?

#include "IOProxyFramebuffer.h"
#include "EDID.h"

//#include <IOKit/IOLib.h>
//#include <IOKit/IOMessage.h>
//#include <IOKit/ndrvsupport/IOMacOSVideo.h>
#include <libkern/OSBase.h>

#define kIOPMIsPowerManagedKey		"IOPMIsPowerManaged"

#define kClassName					"IOProxyFramebuffer"
#define kDebugStr					kClassName ": "
#define DEBUG_CALLS

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

//struct IODisplayModeInformation {
//    UInt32			nominalWidth;
//    UInt32			nominalHeight;
//    IOFixed1616			refreshRate;
//    IOIndex			maxDepthIndex;
//    UInt32			flags;
//    UInt32			reserved[ 4 ];
//};
const IODisplayModeInformation gProxyDisplayModes[] = {
//	{   w,    h,  refresh, maxDepthIndex,               flags, reserved[4]}
	{   0,    0, 60 << 16, kDepth32Bit, kDisplayModeValidFlag},					// make things 1-based to fit in valid IODisplayModeID range 0x1..0x7fffffff
	
	{1920, 1200, 60 << 16, kDepth32Bit, kDisplayModeValidFlag},					// make sure the largest resolution is first, as we use that to specify how much
																				// ram to allocate for the framebuffer

	{1600, 1200, 60 << 16, kDepth32Bit, kDisplayModeValidFlag},
//	{1600, 1200, 75 << 16, kDepth32Bit, kDisplayModeValidFlag},
	{1440,  900, 60 << 16, kDepth32Bit, kDisplayModeValidFlag},
	{1280, 1024, 60 << 16, kDepth32Bit, kDisplayModeValidFlag | kDisplayModeSafeFlag | kDisplayModeDefaultFlag},
	{1280,  854, 60 << 16, kDepth32Bit, kDisplayModeValidFlag},
//	{1280, 1024, 75 << 16, kDepth32Bit, kDisplayModeNeverShowFlag},
//	{1280, 1024, 85 << 16, kDepth32Bit, kDisplayModeNeverShowFlag},
	{1024,  768, 60 << 16, kDepth32Bit, kDisplayModeValidFlag}
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
	bool res = super::start(provider);

#if defined(DEBUG_CALLS)
	IOLog(kDebugStr "start(%s)\n", provider->getMetaClass()->getClassName());
#endif

	fCurrentDisplayMode = 0;
	fCurrentDepth = 0;
	
	fBuffer = NULL;
	
	return res;
}


/*!
	@method		stop
	@abstract	release any framebuffer memory still allocated
	@discussion	paired with start
	@param		<#name#> <#description#>
	@result		<#description#>
*/
void
com_doequalsglory_driver_IOProxyFramebuffer::stop(IOService *provider) {
	if (fBuffer) {
		fBuffer->release();
		fBuffer = NULL;
	}

	fCurrentDepth = 0;
	fCurrentDisplayMode = 0;
	
#if defined(DEBUG_CALLS)
	IOLog(kDebugStr "stop(%s)\n", provider->getMetaClass()->getClassName());
#endif
	super::stop(provider);
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
#if defined(DEBUG_CALLS)
	IOLog(kDebugStr "enableController()\n");
#endif

	// set up initial display mode. should setDisplayMode do the
	// framebuffer memory allocation? I think so
	IODisplayModeID initialDisplayMode;
	IOIndex initialDepth;
	getStartupDisplayMode(&initialDisplayMode, &initialDepth);
	setDisplayMode(initialDisplayMode, initialDepth);

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
	fBuffer = IOBufferMemoryDescriptor::withCapacity(getApertureSize(kLargestDisplayMode, kDepth32Bit), kIODirectionInOut);

	return kIOReturnSuccess;
}


#pragma mark memory management
UInt32 
com_doequalsglory_driver_IOProxyFramebuffer::getApertureSize(IODisplayModeID displayMode, IOIndex depth) {
#if defined(DEBUG_CALLS)
	IOLog(kDebugStr "getApertureSize(displayMode: %d, depth: %d)\n", displayMode, depth);
#endif

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

				how is the connection specified?
    @param		aperture The system will only access the aperture kIOFBSystemAperture.
    @result		an IODeviceMemory instance. A reference will be consumed
				by the caller for each call of this method - the implementatation
				should create a new instance of IODeviceMemory for each
				call, or return one instance with a retain for each call.
*/
IODeviceMemory *
com_doequalsglory_driver_IOProxyFramebuffer::getApertureRange(IOPixelAperture aper) {
#if defined(DEBUG_CALLS)
	IOLog(kDebugStr "getApertureRange(%p)\n", aper);
#endif

	if (fCurrentDisplayMode == 0) {
		return NULL;
	}

	if (aper != kIOFBSystemAperture) {
		return NULL;
	}
	
	IODeviceMemory *deviceMemory = getVRAMRange();    
	                  
	if (deviceMemory == NULL) {
		return NULL;
	}
	
	IODeviceMemory *apertureRange = IODeviceMemory::withSubRange(deviceMemory, 0, getApertureSize(fCurrentDisplayMode, fCurrentDepth));

	deviceMemory->release();													// since getNVRAMRange() does a retain()
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
	IOLog(kDebugStr "getVRAMRange() = %p\n", fBuffer);
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
	IOLog(kDebugStr "getPixelFormats() = %s\n", pixelFormats);
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
	IOLog(kDebugStr "getInformationForDisplayMode(%d)\n", displayMode);
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
	IOLog(kDebugStr "getPixelInformation(displayMode: %d, depth: %d)\n", displayMode, depth);
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
	IOLog(kDebugStr "isConsoleDevice() = %d\n", isConsoleDevice);
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
		IOLog(kDebugStr "getAttribute(attribute: %.4s, *outValue: %u) = %d (kIOReturnSuccess)\n", &attribute, *value, ret);
	} else if (ret == kIOReturnUnsupported) {
		IOLog(kDebugStr "getAttribute(attribute: %.4s, *outValue: %u) = %d (kIOReturnUnsupported)\n", &attribute, *value, ret);
	} else {
		IOLog(kDebugStr "getAttribute(attribute: %.4s, *outValue: %u) = %d\n", &attribute, *value, ret);
	}
#endif

	return kIOReturnSuccess;
}


IOReturn
com_doequalsglory_driver_IOProxyFramebuffer::setAttribute(IOSelect attribute, UInt32 value) {
#if defined(DEBUG_CALLS)
	IOLog(kDebugStr "setAttribute(attribute: %.4s, value: %u)\n", &attribute, value);
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
	@abstract	number of physical monitors plugged in?
	@discussion	ripped from IOFramebuffer.cpp
	@param		<#name#> <#description#>
	@result		<#description#>
--------------------------------+---------------+---------------------------- */
IOItemCount
com_doequalsglory_driver_IOProxyFramebuffer::getConnectionCount(void) {
#if defined(DEBUG_CALLS)
	IOLog(kDebugStr "getConnectionCount() = %d\n", 1);
#endif
	
    return (1);
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
		IOLog(kDebugStr "getAttributeForConnection(connectIndex: %d, attribute: %.4s, *outValue: %u) = %d (kIOReturnSuccess)\n", connectIndex, &attribute, *value, ret);
	} else if (ret == kIOReturnUnsupported) {
		IOLog(kDebugStr "getAttributeForConnection(connectIndex: %d, attribute: %.4s, *outValue: %u) = %d (kIOReturnUnsupported)\n", connectIndex, &attribute, *value, ret);
	} else {
		IOLog(kDebugStr "getAttributeForConnection(connectIndex: %d, attribute: %.4s, *outValue: %u) = %d\n", connectIndex, &attribute, *value, ret);
	}
#endif

	return ret;
}


IOReturn
com_doequalsglory_driver_IOProxyFramebuffer::setAttributeForConnection(IOIndex connectIndex, IOSelect attribute, UInt32 value) {
#if defined(DEBUG_CALLS)
	IOLog(kDebugStr "setAttributeForConnection(connectIndex: %d, attribute: %.4s, value: %u)\n", connectIndex, &attribute, value);
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
	IOLog(kDebugStr "hasDDCConnect(%d)\n", connectIndex);
#endif

	return true;
}


IOReturn
com_doequalsglory_driver_IOProxyFramebuffer::getDDCBlock(IOIndex connectIndex, UInt32 blockNumber, IOSelect blockType,
												IOOptionBits options, UInt8 *data, IOByteCount *length) {
#if defined(DEBUG_CALLS)
	IOLog(kDebugStr "getDDCBlock(connectIndex: %d, blockNumber: %u, blockType: %u, options: %u)\n", connectIndex, blockNumber, blockType, options);
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
    @result		A count of the display modes available.
*/
IOItemCount
com_doequalsglory_driver_IOProxyFramebuffer::getDisplayModeCount() {
	IOItemCount displayModeCount = gProxyDisplayModeCount;
	
#if defined(DEBUG_CALLS)
	IOLog(kDebugStr "getDisplayModeCount() = %u\n", displayModeCount);
#endif

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
#if defined(DEBUG_CALLS)
	IOLog(kDebugStr "getDisplayModes(*outAllDisplayModes: %p)\n", allDisplayModes);
#endif
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
	@function	setDisplayMode
	@abstract	<#brief description#>
	@discussion	<#comprehensive description#>
	@param		<#name#> <#description#>
	@result		<#description#>
*/
IOReturn 
com_doequalsglory_driver_IOProxyFramebuffer::setDisplayMode(IODisplayModeID displayMode, IOIndex depth) {
#if defined(DEBUG_CALLS)
	IOLog(kDebugStr "setDisplayMode(displayMode: %d, depth: %d)\n", displayMode, depth);
#endif

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
	
#if defined(DEBUG_CALLS)
//	IOLog(kDebugStr "getCurrentDisplayMode(*outDisplayMode: %d, *outDepth: %d)\n", *displayMode, *depth);
#endif
	
	return kIOReturnSuccess;
}


IOReturn 
com_doequalsglory_driver_IOProxyFramebuffer::setStartupDisplayMode(IODisplayModeID displayMode, IOIndex depth) {
#if defined(DEBUG_CALLS)
	IOLog(kDebugStr "setStartupDisplayMode(displayMode: %d, depth %d)\n", displayMode, depth);
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
	IOLog(kDebugStr "getStartupDisplayMode(*outDisplayMode: %d, *outDepth: %d)\n", *displayMode, *depth);
#endif

	return kIOReturnSuccess;
}

