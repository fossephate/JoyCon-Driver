#pragma once
#define NIBBLE_SWAP(b) ((((b) & 0xF0) >> 4) | (((b) & 0xf) << 4))
#define _16_BSWAP(x) (((x & 0xFF) << 8) | ((x & 0xFF00) >> 8));
#define WEIRD_SWAP(hi, lo) (((hi & 0x0f) << 4) | ((lo & 0xF0) >> 4))
#define CMD_BLUETOOTH_BUTTON_PRESS 0x3F
#define CMD_POLL_UPDATE1 0x21
#define CMD_POLL_UPDATE2 0x31

struct CmdBTBtn
{
	unsigned char res1;
	unsigned char res2;
	unsigned char dstick;
};

struct CmdBTUpd_lr1
{
	unsigned char res0;
	unsigned char state1;
	unsigned char state2;
};

struct CmdBTUpd_lr2
{
	unsigned char state2;
	unsigned char state1;
	unsigned char res3;
};

struct StickData
{
	// it appears that the X component of the stick data isn't just nibble reversed,
	// specifically, the second nibble of second byte is combined with the first nibble of the first byte
	// to get the correct X stick value:

	// We have to nibble swap and mask off other nibbles, because original code likely used bitfields
	// that truncated to chars, which were then nibble-swapped for little-endianness
	// lo x y hi  when only nibble swapped = x lo hi y == correct endianness for ARM
	unsigned char horiz_lo;
	unsigned char horiz_hi_batt;
	unsigned char vert;
};

struct GyroData
{
	unsigned short pitch;
	unsigned short roll;
	unsigned short yaw;
};

struct AccData
{
	unsigned short x;
	unsigned short y;
	unsigned short z;
};

struct UpdatePacket
{
	unsigned char unknown1;			// 1  (bytes in length)

	union							// 3
	{
		// For whatever reason, these use different fields for updating.
		struct CmdBTUpd_lr1		btupd_lr1;
		struct CmdBTUpd_lr2		btupd_lr2;
	};

	struct StickData stick_lr1; 	// 3
	struct StickData stick_lr2;		// 3

	//unsigned char unknown2;

	// Left-right separation unverified at this time
	struct GyroData gyro_data_lr1;	// 6
	struct GyroData gyro_data_lr2;	// 6

	// Left-right separation unverified at this time
	struct AccData acc_data_lr1;	// 6
	struct AccData acc_data_lr2;	// 6
};

struct Packet // Current bytes included:  36 (~25 unaccounted for)
{
	unsigned char type;				// 1 			0x3f, 0x21, 0x31, 
	union
	{
		struct CmdBTBtn btbtn;
		struct UpdatePacket update;
	};
};
