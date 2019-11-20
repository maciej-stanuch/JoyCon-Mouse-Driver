#pragma once
#include "Joycon.h"

//Returns new Joycon struct
joycon_t make_joycon(const struct hid_device_info* dev) {
	if (dev == NULL) {
		perror("Device cannot be NULL pointer.");
		exit(1);
	}

	joycon_t jc;

	if (dev->product_id == JOYCON_L_BT) {
		const char name[32] = "Joy-Con (L)";
		strcpy_s(jc.name, sizeof(jc.name), name);

		jc.type = JC_LEFT;
	}
	else if (dev->product_id == JOYCON_R_BT) {
		const char name[32] = "Joy-Con (R)";
		strcpy_s(jc.name, sizeof(jc.name), name);

		jc.type = JC_RIGHT;
	}

	jc.serial = _wcsdup(dev->serial_number);

	jc.handle = hid_open_path(dev->path);

	jc.global_packet_number = 0;

	if (jc.handle == NULL) {
		perror("Couldn't open device handle");
		exit(1);
	}

	return jc;
}

//Cleans up after using makeJoycon
void delete_joycon(joycon_t* jc) {
	if (jc != NULL) {
		hid_close(jc->handle);
		jc->handle = NULL;

		free(jc->serial);
		jc->serial = NULL;
	}
}

void send_command(joycon_t* jc, int command, uint8_t* data, int len) {
	unsigned char buf[COMMAND_BUF_SIZE];
	memset(buf, 0, COMMAND_BUF_SIZE);

	/* One byte for command and 63 bytes for data. */

	buf[0] = command;
	if (data && len != 0) {
		memcpy(buf + 1, data, len);
	}

	int result = hid_write(jc->handle, buf, len + 1);

	result = hid_read(jc->handle, buf, COMMAND_BUF_SIZE);

	if (data) {
		memcpy(data, buf, COMMAND_BUF_SIZE);
	}
}

void send_subcommand(joycon_t* jc, int command, int subcommand, uint8_t* data, int len) {
	uint8_t buf[COMMAND_BUF_SIZE];
	memset(buf, 0, COMMAND_BUF_SIZE);

	buf[0] = command; //0x01 for everything, 0x10 for rumble
	buf[1] = jc->global_packet_number;
	jc->global_packet_number++;
	jc->global_packet_number %= 0xF;

	uint8_t rumbleData[8] = { 0x00, 0x01, 0x40, 0x40, 0x00, 0x01, 0x40, 0x40 };
	memcpy(buf + 2, rumbleData, 8);

	buf[10] = subcommand;
	memcpy(buf + 11, data, len);
	int result = hid_write(jc->handle, buf, COMMAND_BUF_SIZE);

	result = hid_read(jc->handle, buf, len);

	if (data) {
		memcpy(data, buf, len);
	}
}

void set_lights(joycon_t* jc, uint8_t bytefield) {
	uint8_t buff[1] = { bytefield };
	send_subcommand(jc, 0x01, 0x30, buff, 1);
}

//basic position X = 2225, Y = 1835
void get_analog_stick_position(joycon_t* jc, uint16_t* X, uint16_t* Y) {
	uint8_t buff[32];
	memset(buff, 0, 32);

	send_subcommand(jc, 0x01, 0x00, buff, 32);

	uint8_t* data = buff + 6;
	*Y = data[0] | ((data[1] & 0xF) << 8);
	*X = (data[1] >> 4) | (data[2] << 4);
}

//byte 1
// 00000001 = DOWN
// 00000010 = RIGHT
// 00000100 = LEFT
// 00001000 = UP
// 00010000 = SL
// 00100000 = SR
//
// byte2
// 00000001 = MINUS
//			= PLUS?
//			= LEFT STICK
//			= RIGHT STICK
//			= HOME
//			= CAPTURE
//			= L/R
//			= ZL/ZR
uint8_t get_buttons_status(joycon_t* jc, buttons_info_t* btn_info_out) {
	uint8_t buff[12];
	memset(buff, 0, 12);


	int res = hid_read_timeout(jc->handle, buff, 12, 10);

	if (res) {
		btn_info_out->DOWN = (buff[1] & 1) != 0;
		btn_info_out->RIGHT = (buff[1] & (1 << 1)) != 0;
		btn_info_out->LEFT = (buff[1] & (1 << 2)) != 0;
		btn_info_out->UP = (buff[1] & (1 << 3)) != 0;
		btn_info_out->SL = (buff[1] & (1 << 4)) != 0;
		btn_info_out->SR = (buff[1] & (1 << 5)) != 0;

		btn_info_out->MINUS_PLUS = ((buff[2] & 1) | (buff[2] & (1 << 1))) != 0;
		btn_info_out->STICK = ((buff[2] & (1 << 2)) | (buff[2] & (1 << 3))) != 0;
		btn_info_out->HOME_CAPTURE = ((buff[2] & (1 << 4)) | (buff[2] & (1 << 5))) != 0;
		btn_info_out->R_L = (buff[2] & (1 << 6)) != 0;
		btn_info_out->ZR_ZL = (buff[2] & (1 << 7)) != 0;

		btn_info_out->STICK_POS = buff[3];
	}
}