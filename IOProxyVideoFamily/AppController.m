/*
 *  AppController.m
 *  IOProxyVideoFamily
 *
 *  Created by thrust on 061203.
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

#import "AppController.h"
#import "UserClientCommon.h"


@implementation AppController

/*! ----------------------------+---------------+-------------------------------
	@method		-IODVCUCOpen
	@abstract	<#brief description#>
	@discussion	this calls the 'open' method in IOProxyVideoCardUserClient
				inside the kernel. though not mandatory, it's good practice to
				use open and close semantics to prevent multiple user space
				applications from communicating with your driver at the same
				time.
	@param		<#name#> <#description#>
	@result		<#description#>
--------------------------------+---------------+---------------------------- */
-(kern_return_t)
IOProxyVideoCardUserClientOpen {
    kern_return_t kernResult = IOConnectMethodScalarIScalarO(dataPort, kMyUserClientOpen, 0, 0);
    
    return kernResult;
}


/*! ----------------------------+---------------+-------------------------------
	@method		-IODVCUCClose
	@abstract	<#brief description#>
	@discussion	this calls the 'close' method in IOProxyVideoCardUserClient
				inside the kernel. in this case calling 'close' isn't necessary
				since the call to IOServiceClose() will also call 'close' for us.
	@param		<#name#> <#description#>
	@result		<#description#>
--------------------------------+---------------+---------------------------- */
-(kern_return_t)
IOProxyVideoCardUserClientClose {
    kern_return_t kernResult = IOConnectMethodScalarIScalarO(dataPort, kMyUserClientClose, 0, 0);
	
    return kernResult;
}


-(int)
IOProxyVideoCardConnectionCount {
    int					resultNumber;
    kern_return_t		kernResult;
	
    kernResult = IOConnectMethodScalarIScalarO(dataPort,						// an io_connect_t returned from IOServiceOpen().
											   kConnectionCountMethod,			// an index to the function in the Kernel.
											   0,								// the number of scalar input values.
											   1,								// the number of scalar output values.
											   &resultNumber					// a scalar output parameter.
											   );

	if (kernResult == KERN_SUCCESS) {
		[properties setValue:[NSNumber numberWithInt:resultNumber] forKey:@"connectionCount"];
	}
	
    return kernResult;
}


/*! ----------------------------+---------------+-------------------------------
	@method		-IODVCConnectionPropertiesAtIndex
	@abstract	<#brief description#>
	@discussion	<#comprehensive description#>
	@param		<#name#> <#description#>
	@result		<#description#>
--------------------------------+---------------+---------------------------- */
-(kern_return_t)
IOProxyVideoCardConnectionPropertiesAtIndex:(int)index {
	ConnectionStruct			connection;
	IOByteCount					structSize		= sizeof(ConnectionStruct);
	kern_return_t				kernResult;
	
	kernResult = IOConnectMethodScalarIStructureO(dataPort,						// an io_connect_t returned from IOServiceOpen()
												  kConnectionPropertiesMethod,	// an index to the function in the kernel
												  1,							// the number of scalar input values
												  &structSize,					// the size of the struct output parameter
												  index,						// a scalar input parameter
												  &connection					// a pointer to a struct output parameter
												  );

	if (kernResult == KERN_SUCCESS) {
		NSLog(@"kConnectionPropertiesMethod was successful for index %d", index);
		NSLog(@"    enabled=%d", connection.enabled);
		NSLog(@"     maxRes=%d,%d", connection.maxWidth, connection.maxHeight);
		NSLog(@"        res=%d,%d", connection.width, connection.height);
		
		NSMutableDictionary *connectionDict = [NSMutableDictionary dictionaryWithObjectsAndKeys:
			[NSNumber numberWithInt:index], @"index",
			[NSNumber numberWithBool:connection.enabled], @"enabled",
			[NSString stringWithCString:connection.name], @"name",
			[NSNumber numberWithInt:connection.maxWidth], @"maxWidth",
			[NSNumber numberWithInt:connection.maxHeight], @"maxHeight",
			[NSNumber numberWithInt:connection.width], @"width",
			[NSNumber numberWithInt:connection.height], @"height", nil];
		
		NSMutableArray *connections = [self mutableArrayValueForKey:@"framebuffers"];
		[connections addObject:connectionDict];
	} else {
		NSLog(@"kConnectionPropertiesMethod at index %d failed: %d", index, kernResult);
	}
	
	return kernResult;
}


/*! ----------------------------+---------------+-------------------------------
	@method		-IODVCSetConnectionProperties: atIndex:
	@abstract	<#brief description#>
	@discussion	<#comprehensive description#>
	@param		<#name#> <#description#>
	@result		<#description#>
--------------------------------+---------------+---------------------------- */
-(kern_return_t)
IOProxyVideoCardSetConnectionProperties:(ConnectionStruct)connection atIndex:(int)index {
	IOByteCount					structSize		= sizeof(ConnectionStruct);
	kern_return_t				kernResult;

	kernResult = IOConnectMethodScalarIStructureI(dataPort,						// an io_connect_t returned from IOServiceOpen()
												  kSetConnectionPropertiesMethod,	// an index to the function in the kernel
												  1,							// the number of scalar input values
												  structSize,					// the size of the struct input parameter
												  index,						// a scalar input parameter
												  &connection					// a pointer to a struct input parameter
												  );
	
	if (kernResult == KERN_SUCCESS) {
		NSLog(@"kSetConnectionPropertiesMethod was successful");
	}
	
	return kernResult;
}

/*! ----------------------------+---------------+-------------------------------
	@method		-IODVCApplyChanges
	@abstract	<#brief description#>
	@discussion	<#comprehensive description#>
	@param		<#name#> <#description#>
	@result		<#description#>
--------------------------------+---------------+---------------------------- */
-(kern_return_t)
IOProxyVideoCardApplyChanges {
    kern_return_t kernResult = IOConnectMethodScalarIScalarO(dataPort, kApplyChangesMethod, 0, 0);
    
	if (kernResult != KERN_SUCCESS) {
		NSLog(@"kApplyChangesMethod failed with error %X", kernResult);
	}
	
    return kernResult;
}

#pragma mark -

-(id)
init {
	self = [super init];
	if (self) {
		properties = [[NSMutableDictionary alloc] init];
		framebuffers = [[NSMutableArray alloc] init];
		
		[properties setValue:[NSNumber numberWithInt:0] forKeyPath:@"connectionCount"];
	}
	return self;
}


-(void)
dealloc {
	[framebuffers release];
	[properties release];
	
	[super dealloc];
}


/*! ----------------------------+---------------+-------------------------------
	@method		«method»
	@abstract	<#brief description#>
	@discussion	<#comprehensive description#>
	@param		<#name#> <#description#>
	@result		<#description#>
--------------------------------+---------------+---------------------------- */
-(void)
awakeFromNib {
	NSLog(@"awakeFromNib");
	
    kern_return_t				kernResult; 
    mach_port_t					masterPort;
    io_service_t				serviceObject;
    io_iterator_t				iterator;
    CFDictionaryRef				classToMatch;	
    
    kernResult = IOMasterPort(MACH_PORT_NULL, &masterPort);						// Returns the mach port used to initiate communication with IOKit
    
    if (kernResult != KERN_SUCCESS) {
        NSLog(@"IOMasterPort returned %d\n", kernResult);
        return;
    }
    
    classToMatch = IOServiceMatching("com_doequalsglory_driver_IOProxyVideoCard");
    
    if (classToMatch == NULL) {
        NSLog(@"IOServiceMatching returned a NULL dictionary.");
        return;
    }
    
    // This creates an io_iterator_t of all instances of our drivers class that exist in the IORegistry.
    kernResult = IOServiceGetMatchingServices(masterPort, classToMatch, &iterator);
    
    if (kernResult != KERN_SUCCESS) {
        NSLog(@"IOServiceGetMatchingServices returned %d", kernResult);
        return;
    }
	
    serviceObject = IOIteratorNext(iterator);									// Get the first item in the iterator.
    
    IOObjectRelease(iterator);													// Release the io_iterator_t now that we're done with it.
    
    if (!serviceObject) {
        NSLog(@"Couldn't find any matches.");
        return;
    }
    
    kernResult = IOServiceOpen(serviceObject, mach_task_self(), 0, &dataPort);	// This call will cause the user client to be instantiated.
    
    IOObjectRelease(serviceObject);												// Release the io_service_t now that we're done with it.
    	
    if (kernResult != KERN_SUCCESS) {
        NSLog(@"IOServiceOpen returned %x", kernResult);
        return;
    }
	
    kernResult = [self IOProxyVideoCardUserClientOpen];							// This shows an example of calling a user client's open method.
    
    if (kernResult != KERN_SUCCESS) {
        NSLog(@"IOProxyVideoCardUserClientOpen returned %d", kernResult);
        IOServiceClose(dataPort);												// This closes the connection to our user client and destroys the connect handle.
        return;
    }
    
	// if we get this far then we have a good connection to the UserClient,
	// so start pulling info
	kernResult = [self IOProxyVideoCardConnectionCount];
	
	if (kernResult != KERN_SUCCESS) {
		NSLog(@"IOProxyVideoCardConnectionCount returned %d", kernResult);
	}
	
	// populate framebuffer array
	int i;
	int count = [[properties valueForKey:@"connectionCount"] intValue];
	for (i = 0; i < count; i++) {
		[self IOProxyVideoCardConnectionPropertiesAtIndex:i];
	}
	
	// enable apply button
	//[self setValue:[NSNumber numberWithBool:NO] forKey:@"framebufferEnabled"];
}


-(void)
applicationWillTerminate:(NSNotification *)aNotification {
	NSLog(@"applicationWillTerminate");
	
    kern_return_t	kernResult; 
	
    kernResult = [self IOProxyVideoCardUserClientClose];						// This shows an example of calling a user client's close method.
	
    if (kernResult != KERN_SUCCESS) {
        NSLog(@"IOProxyVideoCardUserClientClose returned %d\n\n", kernResult);
    }
	
    kernResult = IOServiceClose(dataPort);										// This closes the connection to our user client and destroys the connect handle.
    
    if (kernResult != KERN_SUCCESS) {
        NSLog(@"IOServiceClose returned %d", kernResult);
    }
}


#pragma mark bindings

-(BOOL)
buttonEnabled {
	NSLog(@"buttonEnabled");
	return YES;
}

//-(void)
//setFramebufferEnabled:(BOOL)state {
//	framebufferEnabled = state;
//	if (framebufferEnabled) {
//		[self setValue:@"Disable" forKey:@"buttonTitle"];
//	} else {
//		[self setValue:@"Enable" forKey:@"buttonTitle"];
//	}
//}

-(void)
applyButtonPressed {
	NSLog(@"apply");
	
	// set the properties in the driver
	NSEnumerator *connectionEnumerator = [framebuffers objectEnumerator];
	NSMutableDictionary *connection;
	
	while (connection = [connectionEnumerator nextObject]) {
		ConnectionStruct connectionStruct;
		
		connectionStruct.enabled = [[connection objectForKey:@"enabled"] boolValue];
		NSString *name = [connection objectForKey:@"name"];
		[name getCString:connectionStruct.name maxLength:sizeof(connectionStruct.name)];
		connectionStruct.width = [[connection objectForKey:@"width"] intValue];
		connectionStruct.height = [[connection objectForKey:@"height"] intValue];
		
		[self IOProxyVideoCardSetConnectionProperties:connectionStruct atIndex:[[connection objectForKey:@"index"] intValue]];
	}
	
	// whack our properties
	[framebuffers removeAllObjects];
	
	// reload our properties from the driver
	int i;
	int count = [[properties valueForKey:@"connectionCount"] intValue];
	for (i = 0; i < count; i++) {
		[self IOProxyVideoCardConnectionPropertiesAtIndex:i];
	}
	
	// finally apply the changes to the driver
	[self IOProxyVideoCardApplyChanges];
}

@end
