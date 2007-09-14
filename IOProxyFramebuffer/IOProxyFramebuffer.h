#ifndef __IOPROXYFRAMEBUFFER_H__
#define __IOPROXYFRAMEBUFFER_H__

#include <IOKit/graphics/IOFramebuffer.h>
#include <IOKit/IOBufferMemoryDescriptor.h>


// if this is an IOFramebuffer instead of an IOService, free fails and
// we can't unload. why is that?
class com_doequalsglory_driver_IOProxyFramebuffer : public IOFramebuffer {
	OSDeclareDefaultStructors(com_doequalsglory_driver_IOProxyFramebuffer)
	
public:
	// generic kext setup & teardown
	//virtual bool com_doequalsglory_driver_IOProxyFramebuffer::init(OSDictionary *properties = 0);
	//virtual bool com_doequalsglory_driver_IOProxyFramebuffer::attach(IOService *provider);
	//virtual IOService *com_doequalsglory_driver_IOProxyFramebuffer::probe(IOService *provider, SInt32 *score);
	//virtual void com_doequalsglory_driver_IOProxyFramebuffer::detach(IOService *provider);
	//virtual void com_doequalsglory_driver_IOProxyFramebuffer::free();
	
	virtual bool com_doequalsglory_driver_IOProxyFramebuffer::start(IOService *provider);
	virtual void com_doequalsglory_driver_IOProxyFramebuffer::stop(IOService *provider);
	
	//virtual bool com_doequalsglory_driver_IOProxyFramebuffer::requestTerminate( IOService * provider, IOOptionBits options );
	
	//virtual IOReturn com_doequalsglory_driver_IOProxyFramebuffer::open(void);
	// close is not being called
	//virtual void com_doequalsglory_driver_IOProxyFramebuffer::close(void);
	
	//
	// IOFramebuffer specific methods
	//
	
	// perform first-time setup of the framebuffer
	virtual IOReturn com_doequalsglory_driver_IOProxyFramebuffer::enableController(void);
	
	// memory management
	virtual UInt32				com_doequalsglory_driver_IOProxyFramebuffer::getApertureSize(IODisplayModeID, IOIndex);
	virtual IODeviceMemory *	com_doequalsglory_driver_IOProxyFramebuffer::getApertureRange(IOPixelAperture);
	virtual IODeviceMemory *	com_doequalsglory_driver_IOProxyFramebuffer::getVRAMRange(void);
	
	// framebuffer info
	virtual const char *		com_doequalsglory_driver_IOProxyFramebuffer::getPixelFormats();
	virtual IOReturn			com_doequalsglory_driver_IOProxyFramebuffer::getInformationForDisplayMode(IODisplayModeID, IODisplayModeInformation *);
	virtual UInt64				com_doequalsglory_driver_IOProxyFramebuffer::getPixelFormatsForDisplayMode(IODisplayModeID, IOIndex);
	virtual IOReturn			com_doequalsglory_driver_IOProxyFramebuffer::getPixelInformation(IODisplayModeID, IOIndex, IOPixelAperture, IOPixelInformation *);
	
	virtual bool				com_doequalsglory_driver_IOProxyFramebuffer::isConsoleDevice(void);
	
	virtual IOReturn			com_doequalsglory_driver_IOProxyFramebuffer::getTimingInfoForDisplayMode(IODisplayModeID, IOTimingInformation *);
	
	virtual IOReturn			com_doequalsglory_driver_IOProxyFramebuffer::getAttribute(IOSelect, UInt32 *);
	virtual IOReturn			com_doequalsglory_driver_IOProxyFramebuffer::setAttribute(IOSelect, UInt32);
	
	// connection info
	virtual IOItemCount			com_doequalsglory_driver_IOProxyFramebuffer::getConnectionCount(void);
	
	virtual IOReturn			com_doequalsglory_driver_IOProxyFramebuffer::getAttributeForConnection(IOIndex, IOSelect, UInt32 *);
	virtual IOReturn			com_doequalsglory_driver_IOProxyFramebuffer::setAttributeForConnection(IOIndex, IOSelect, UInt32);
	
	virtual bool				com_doequalsglory_driver_IOProxyFramebuffer::hasDDCConnect(IOIndex);
	virtual IOReturn			com_doequalsglory_driver_IOProxyFramebuffer::getDDCBlock(IOIndex, UInt32, IOSelect, IOOptionBits, UInt8 *, IOByteCount *);
	
	// display mode accessors
	virtual IOItemCount			com_doequalsglory_driver_IOProxyFramebuffer::getDisplayModeCount();
	virtual IOReturn			com_doequalsglory_driver_IOProxyFramebuffer::getDisplayModes(IODisplayModeID *);
	
	virtual IOReturn			com_doequalsglory_driver_IOProxyFramebuffer::setDisplayMode(IODisplayModeID, IOIndex);
	virtual IOReturn			com_doequalsglory_driver_IOProxyFramebuffer::getCurrentDisplayMode(IODisplayModeID *, IOIndex *);
	
	virtual IOReturn			com_doequalsglory_driver_IOProxyFramebuffer::setStartupDisplayMode(IODisplayModeID, IOIndex);
    virtual IOReturn			com_doequalsglory_driver_IOProxyFramebuffer::getStartupDisplayMode(IODisplayModeID *, IOIndex *);
	
private:
	IOBufferMemoryDescriptor *	fBuffer;
	
	IODisplayModeID				fCurrentDisplayMode;
	IOIndex						fCurrentDepth;
	
	UInt32						fPowerState;
};

#endif