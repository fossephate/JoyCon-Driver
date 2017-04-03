/*++

Copyright (c) Shaul Eizikovich.  All rights reserved.

    THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY
    KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR
    PURPOSE.

Module Name:

    public.h
    
Abstract:

    Public header file for the vJoy project
	Developpers that need to interface with vJoy need to include this file

Author:


Environment:

    kernel mode and User mode

Notes:


Revision History:


--*/
#ifndef _PUBLIC_H
#define _PUBLIC_H

// Compilation directives
#define PPJOY_MODE
#undef PPJOY_MODE	// Comment-out for compatibility mode

#ifdef PPJOY_MODE
#include "PPJIoctl.h"
#endif

#include <INITGUID.H>	// Definitions for controlling GUID initialization

// Sideband comunication with vJoy Device
//{781EF630-72B2-11d2-B852-00C04FAD5101}
DEFINE_GUID(GUID_DEVINTERFACE_VJOY, 0x781EF630, 0x72B2, 0x11d2, 0xB8, 0x52, 0x00, 0xC0, 0x4F, 0xAD, 0x51, 0x01);

//
// Usage example:
//		CreateFile(TEXT("\\\\.\\vJoy"), GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
#ifdef PPJOY_MODE
#define DEVICENAME_STRING			"PPJoyIOCTL1"
#else
#define DEVICENAME_STRING			"vJoy"
#endif
#define NTDEVICE_NAME_STRING		"\\Device\\"DEVICENAME_STRING
#define SYMBOLIC_NAME_STRING		"\\DosDevices\\"DEVICENAME_STRING
#define	DOS_FILE_NAME				"\\\\.\\"DEVICENAME_STRING
#define VJOY_INTERFACE				L"Device_"

// Version parts
#define VER_X_	0
#define VER_H_	2
#define VER_M_	1
#define VER_L_	8

#define STRINGIFY_1(x)   #x
#define STRINGIFY(x)     STRINGIFY_1(x)
#define PASTE(x, y) x##y
#define MAKEWIDE(x) PASTE(L,x)

// Device Attributes
//
#define VENDOR_N_ID		0x1234
#define	PRODUCT_N_ID	0xBEAD
#define	VERSION_N	(VER_L_ + 0x10*VER_M_ + 0x100*VER_H_ + 0x1000*VER_X_)

// Device Strings
//
#define VENDOR_STR_ID		L"Shaul Eizikovich"
#define PRODUCT_STR_ID		L"vJoy - Virtual Joystick"
#define	SERIALNUMBER_STR	MAKEWIDE(STRINGIFY(VER_H_)) L"." MAKEWIDE(STRINGIFY(VER_M_)) L"."  MAKEWIDE(STRINGIFY(VER_L_))

// Function codes;
//#define LOAD_POSITIONS	0x910
//#define GETATTRIB		0x911
// #define GET_FFB_DATA	0x00222912	// METHOD_OUT_DIRECT + FILE_DEVICE_UNKNOWN	+ FILE_ANY_ACCESS
//#define SET_FFB_STAT	0x913	 // METHOD_NEITHER
//#define GET_FFB_STAT	0x916

#define F_LOAD_POSITIONS	0x910
#define F_GETATTRIB			0x911
#define F_GET_FFB_DATA		0x912
#define F_SET_FFB_STAT		0x913
#define F_GET_FFB_STAT		0x916
#define F_GET_DEV_INFO      0x917
#define F_IS_DRV_FFB_CAP	0x918
#define F_IS_DRV_FFB_EN		0x919
#define F_GET_DRV_DEV_MAX	0x91A
#define F_GET_DRV_DEV_EN	0x91B
#define F_IS_DEV_FFB_START	0x91C
#define F_GET_DEV_STAT		0x91D
#define F_GET_DRV_INFO		0x91E
#define F_RESET_DEV			0x91F
#define F_GET_POSITIONS		0x920

// IO Device Control codes;
#define IOCTL_VJOY_GET_ATTRIB		CTL_CODE (FILE_DEVICE_UNKNOWN, GETATTRIB, METHOD_BUFFERED, FILE_WRITE_ACCESS)
#define LOAD_POSITIONS	            CTL_CODE (FILE_DEVICE_UNKNOWN, F_LOAD_POSITIONS, METHOD_BUFFERED, FILE_WRITE_ACCESS)
#define GET_POSITIONS	            CTL_CODE (FILE_DEVICE_UNKNOWN, F_GET_POSITIONS, METHOD_BUFFERED, FILE_READ_ACCESS)
#define GET_FFB_DATA	            CTL_CODE (FILE_DEVICE_UNKNOWN, F_GET_FFB_DATA, METHOD_OUT_DIRECT, FILE_ANY_ACCESS)
#define SET_FFB_STAT	            CTL_CODE (FILE_DEVICE_UNKNOWN, F_SET_FFB_STAT, METHOD_NEITHER, FILE_ANY_ACCESS)
#define GET_FFB_STAT	            CTL_CODE (FILE_DEVICE_UNKNOWN, F_GET_FFB_STAT, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define GET_DEV_INFO			    CTL_CODE (FILE_DEVICE_UNKNOWN, F_GET_DEV_INFO, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IS_DRV_FFB_CAP			    CTL_CODE (FILE_DEVICE_UNKNOWN, F_IS_DRV_FFB_CAP, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IS_DRV_FFB_EN			    CTL_CODE (FILE_DEVICE_UNKNOWN, F_IS_DRV_FFB_EN, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define GET_DRV_DEV_MAX			    CTL_CODE (FILE_DEVICE_UNKNOWN, F_GET_DRV_DEV_MAX, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define GET_DRV_DEV_EN			    CTL_CODE (FILE_DEVICE_UNKNOWN, F_GET_DRV_DEV_EN, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IS_DEV_FFB_START			CTL_CODE (FILE_DEVICE_UNKNOWN, F_IS_DEV_FFB_START, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define GET_DEV_STAT				CTL_CODE (FILE_DEVICE_UNKNOWN, F_GET_DEV_STAT, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define GET_DRV_INFO				CTL_CODE (FILE_DEVICE_UNKNOWN, F_GET_DRV_INFO, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define RESET_DEV					CTL_CODE (FILE_DEVICE_UNKNOWN, F_RESET_DEV, METHOD_BUFFERED, FILE_WRITE_ACCESS)

#ifndef __HIDPORT_H__
// Copied from hidport.h
#define IOCTL_HID_SET_FEATURE	0xB0191
#define IOCTL_HID_WRITE_REPORT	0xB000F

#define MAX_N_DEVICES	16 // Maximum number of vJoy devices


typedef struct _HID_DEVICE_ATTRIBUTES {

    ULONG           Size;
    //
    // sizeof (struct _HID_DEVICE_ATTRIBUTES)
    //

    //
    // Vendor ids of this hid device
    //
    USHORT          VendorID;
    USHORT          ProductID;
    USHORT          VersionNumber;
    USHORT          Reserved[11];

} HID_DEVICE_ATTRIBUTES, * PHID_DEVICE_ATTRIBUTES;
#endif

// Device Type
//enum DevType { vJoy, vXbox };

// Error levels for status report
enum ERRLEVEL {INFO, WARN, ERR, FATAL, APP};
// Status report function prototype
#ifdef WINAPI
typedef BOOL (WINAPI *StatusMessageFunc)(void * output, TCHAR * buffer, enum ERRLEVEL level);
#endif

///////////////////////////////////////////////////////////////

/////////////////////// Joystick Position ///////////////////////
//
// This structure holds data that is passed to the device from
// an external application such as SmartPropoPlus.
//
// Usage example:
//	JOYSTICK_POSITION iReport;
//	:
//	DeviceIoControl (hDevice, 100, &iReport, sizeof(HID_INPUT_REPORT), NULL, 0, &bytes, NULL)
typedef struct _JOYSTICK_POSITION
{
	BYTE	bDevice;	// Index of device. 1-based.
	LONG	wThrottle;
	LONG	wRudder;
	LONG	wAileron;
	LONG	wAxisX;
	LONG	wAxisY;
	LONG	wAxisZ;
	LONG	wAxisXRot;
	LONG	wAxisYRot;
	LONG	wAxisZRot;
	LONG	wSlider;
	LONG	wDial;
	LONG	wWheel;
	LONG	wAxisVX;
	LONG	wAxisVY;
	LONG	wAxisVZ;
	LONG	wAxisVBRX;
	LONG	wAxisVBRY;
	LONG	wAxisVBRZ;
	LONG	lButtons;	// 32 buttons: 0x00000001 means button1 is pressed, 0x80000000 -> button32 is pressed
	DWORD	bHats;		// Lower 4 bits: HAT switch or 16-bit of continuous HAT switch
	DWORD	bHatsEx1;	// 16-bit of continuous HAT switch
	DWORD	bHatsEx2;	// 16-bit of continuous HAT switch
	DWORD	bHatsEx3;	// 16-bit of continuous HAT switch
} JOYSTICK_POSITION, *PJOYSTICK_POSITION;

// Superset of JOYSTICK_POSITION
// Extension of JOYSTICK_POSITION with Buttons 33-128 appended to the end of the structure.
typedef struct _JOYSTICK_POSITION_V2
{
	/// JOYSTICK_POSITION
	BYTE	bDevice;	// Index of device. 1-based.
	LONG	wThrottle;
	LONG	wRudder;
	LONG	wAileron;
	LONG	wAxisX;
	LONG	wAxisY;
	LONG	wAxisZ;
	LONG	wAxisXRot;
	LONG	wAxisYRot;
	LONG	wAxisZRot;
	LONG	wSlider;
	LONG	wDial;
	LONG	wWheel;
	LONG	wAxisVX;
	LONG	wAxisVY;
	LONG	wAxisVZ;
	LONG	wAxisVBRX;
	LONG	wAxisVBRY;
	LONG	wAxisVBRZ;
	LONG	lButtons;	// 32 buttons: 0x00000001 means button1 is pressed, 0x80000000 -> button32 is pressed
	DWORD	bHats;		// Lower 4 bits: HAT switch or 16-bit of continuous HAT switch
	DWORD	bHatsEx1;	// Lower 4 bits: HAT switch or 16-bit of continuous HAT switch
	DWORD	bHatsEx2;	// Lower 4 bits: HAT switch or 16-bit of continuous HAT switch
	DWORD	bHatsEx3;	// Lower 4 bits: HAT switch or 16-bit of continuous HAT switch LONG lButtonsEx1; // Buttons 33-64
	
	/// JOYSTICK_POSITION_V2 Extenssion
	LONG lButtonsEx1; // Buttons 33-64
	LONG lButtonsEx2; // Buttons 65-96
	LONG lButtonsEx3; // Buttons 97-128
} JOYSTICK_POSITION_V2, *PJOYSTICK_POSITION_V2;


// HID Descriptor definitions - Axes
#define HID_USAGE_X		0x30
#define HID_USAGE_Y		0x31
#define HID_USAGE_Z		0x32
#define HID_USAGE_RX	0x33
#define HID_USAGE_RY	0x34
#define HID_USAGE_RZ	0x35
#define HID_USAGE_SL0	0x36
#define HID_USAGE_SL1	0x37
#define HID_USAGE_WHL	0x38
#define HID_USAGE_POV	0x39

// HID Descriptor definitions - FFB Effects
#define HID_USAGE_CONST 0x26    //    Usage ET Constant Force
#define HID_USAGE_RAMP  0x27    //    Usage ET Ramp
#define HID_USAGE_SQUR  0x30    //    Usage ET Square
#define HID_USAGE_SINE  0x31    //    Usage ET Sine
#define HID_USAGE_TRNG  0x32    //    Usage ET Triangle
#define HID_USAGE_STUP  0x33    //    Usage ET Sawtooth Up
#define HID_USAGE_STDN  0x34    //    Usage ET Sawtooth Down
#define HID_USAGE_SPRNG 0x40    //    Usage ET Spring
#define HID_USAGE_DMPR  0x41    //    Usage ET Damper
#define HID_USAGE_INRT  0x42    //    Usage ET Inertia
#define HID_USAGE_FRIC  0x43    //    Usage ET Friction


// HID Descriptor definitions - FFB Report IDs
#define HID_ID_STATE	0x02	// Usage PID State report
#define HID_ID_EFFREP	0x01	// Usage Set Effect Report
#define HID_ID_ENVREP	0x02	// Usage Set Envelope Report
#define HID_ID_CONDREP	0x03	// Usage Set Condition Report
#define HID_ID_PRIDREP	0x04	// Usage Set Periodic Report
#define HID_ID_CONSTREP	0x05	// Usage Set Constant Force Report
#define HID_ID_RAMPREP	0x06	// Usage Set Ramp Force Report
#define HID_ID_CSTMREP	0x07	// Usage Custom Force Data Report
#define HID_ID_SMPLREP	0x08	// Usage Download Force Sample
#define HID_ID_EFOPREP	0x0A	// Usage Effect Operation Report
#define HID_ID_BLKFRREP	0x0B	// Usage PID Block Free Report
#define HID_ID_CTRLREP	0x0C	// Usage PID Device Control
#define HID_ID_GAINREP	0x0D	// Usage Device Gain Report
#define HID_ID_SETCREP	0x0E	// Usage Set Custom Force Report
#define HID_ID_NEWEFREP	0x01	// Usage Create New Effect Report
#define HID_ID_BLKLDREP	0x02	// Usage Block Load Report
#define HID_ID_POOLREP	0x03	// Usage PID Pool Report

#endif
