/*
 *  EDID.h
 *  IOProxyVideoFamily
 *
 *  Created by thrust on 061031.
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