/*
 *  edid.h
 *  ioproxyfb
 *
 *  Created by thrust on 10/31/06.
 *  Copyright 2006 __MyCompanyName__. All rights reserved.
 *
 */

#if !defined(__EDID_H__)
#define __EDID_H__

struct CompleteSerialNumber {
	UInt8	manufacturerID[2];
	UInt8	productIDCode[2];
	UInt8	serialNumber[4];
	UInt8	weekOfManufacture;
	UInt8	yearOfManufacture;
};

struct BasicDisplayParameters {
	UInt8	videoInputDefinition;
	UInt8	maxHImageSize;
	UInt8	maxVImageSize;
	UInt8	gamma;
	UInt8	powerMgmtAndFeatures;
};

struct ChromaInfo {
	UInt8					info[10];
};

struct StandardTiming {
	UInt8					horizontalRes;
	UInt8					verticalRes;
};

struct DescriptorBlock1 {
	UInt16					pixelClock;
	UInt8					reserved0;
	UInt8					blockType;
	UInt8					reserved1;
	UInt8					info[13];
};

struct DescriptorBlock2 {
	UInt8					info[18];
};

struct DescriptorBlock3 {
	UInt8					info[18];
};

struct DescriptorBlock4 {
	UInt8					info[18];
};

struct EDID {
    UInt8					header[8];
    CompleteSerialNumber	serialNumber;
    UInt8					EDIDVersionNumber;
    UInt8					EDIDRevisionNumber;
	BasicDisplayParameters	displayParams;
    ChromaInfo				chromaInfo;
	UInt8					establishedTiming1;
	UInt8					establishedTiming2;
	UInt8					manufacturerReservedTiming;
    StandardTiming			standardTimings[8];
	DescriptorBlock1		descriptorBlock1;
	DescriptorBlock2		descriptorBlock2;
	DescriptorBlock3		descriptorBlock3;
	DescriptorBlock4		descriptorBlock4;
    UInt8					extension;											// 0 in EDID 1.1
    UInt8					checksum;
};

#endif