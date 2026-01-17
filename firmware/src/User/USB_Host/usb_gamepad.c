#include <usb_gamepad.h>
#include <stdio.h>
#include <stdlib.h>

#define JOYSTICK_AXIS_MIN           0
#define JOYSTICK_AXIS_MID           127
#define JOYSTICK_AXIS_MAX           255
#define JOYSTICK_AXIS_TRIGGER_MIN   64
#define JOYSTICK_AXIS_TRIGGER_MAX   192

#define JOY_RIGHT       0x01
#define JOY_LEFT        0x02
#define JOY_DOWN        0x04
#define JOY_UP          0x08
#define JOY_BTN_SHIFT   4
#define JOY_BTN1        0x10  // Bit 4
#define JOY_BTN2        0x20  // Bit 5
#define JOY_BTN3        0x40  // Bit 6
#define JOY_BTN4        0x80  // Bit 7 (now used)
#define JOY_MOVE        (JOY_RIGHT|JOY_LEFT|JOY_UP|JOY_DOWN)



HID_gamepad_Info_TypeDef    gamepad_info;
static uint8_t 			gamepad_report_data[64];




HID_gamepad_Info_TypeDef *GetGamepadInfo(Interface *Itf)
{
	//refresh value of joymap and return value
	if(GamepadDecode(Itf)== USB_OK)
	{

		return &gamepad_info;
	}
	else
	{
		return NULL;
	}
}



USB_Status GamepadDecode(Interface *Itf)
{


	  if (Itf->HidRptLen == 0U)
	  {
	    return USB_FAIL;
	  }

	  if (FifoRead(&Itf->buffer, &gamepad_report_data, Itf->HidRptLen) !=0)
	  {


		uint8_t jmap = 0;
		uint8_t btn = 0;
		uint8_t btn_extra = 0;
		int16_t a[2];
		uint8_t i;



		hid_report_t conf = Itf->HIDRptDesc;
				//HID_Handle->HID_Desc.RptDesc;

		// skip report id if present
		uint8_t *p = gamepad_report_data+(conf.report_id?1:0);


		//process axis
		// two axes ...
				for(i=0;i<2;i++) {
					// if logical minimum is > logical maximum then logical minimum
					// is signed. This means that the value itself is also signed
					int is_signed = conf.joystick_mouse.axis[i].logical.min >
					conf.joystick_mouse.axis[i].logical.max;
					a[i] = collect_bits(p, conf.joystick_mouse.axis[i].offset,
								conf.joystick_mouse.axis[i].size, is_signed);
				}

		// Process first 7 buttons (for CD32 support: 3 fire buttons + 4 directional equivalent)
		// Buttons 0-3 go to btn bits 0-3
		// Buttons 4-6 go to btn bits 4-6
		// Button 7 (if exists) can be the 4th button for compatibility
		for(i=0;i<7;i++)
			if(p[conf.joystick_mouse.button[i].byte_offset] &
			 conf.joystick_mouse.button[i].bitmask) btn |= (1<<i);

		// ... and the remaining extra buttons (buttons 7-11)
		for(i=7;i<12;i++)
			if(p[conf.joystick_mouse.button[i].byte_offset] &
			 conf.joystick_mouse.button[i].bitmask) btn_extra |= (1<<(i-7));



	for(i=0;i<2;i++) {

		int hrange = (conf.joystick_mouse.axis[i].logical.max - abs(conf.joystick_mouse.axis[i].logical.min)) / 2;
		int dead = hrange/63;

		if (a[i] < conf.joystick_mouse.axis[i].logical.min) a[i] = conf.joystick_mouse.axis[i].logical.min;
		else if (a[i] > conf.joystick_mouse.axis[i].logical.max) a[i] = conf.joystick_mouse.axis[i].logical.max;

		a[i] = a[i] - (abs(conf.joystick_mouse.axis[i].logical.min) + conf.joystick_mouse.axis[i].logical.max) / 2;

		hrange -= dead;
		if (a[i] < -dead) a[i] += dead;
		else if (a[i] > dead) a[i] -= dead;
		else a[i] = 0;

		a[i] = (a[i] * 127) / hrange;

		if (a[i] < -127) a[i] = -127;
		else if (a[i] > 127) a[i] = 127;

		a[i]=a[i]+127; // mist wants a value in the range [0..255]
	}

				if(a[0] < JOYSTICK_AXIS_TRIGGER_MIN) jmap |= JOY_LEFT;
				if(a[0] > JOYSTICK_AXIS_TRIGGER_MAX) jmap |= JOY_RIGHT;
				if(a[1] < JOYSTICK_AXIS_TRIGGER_MIN) jmap |= JOY_UP;
				if(a[1] > JOYSTICK_AXIS_TRIGGER_MAX) jmap |= JOY_DOWN;
				
				// Map buttons to gamepad_data byte:
				// Bits 0-3: Directional (jmap already has these)
				// Bits 4-7: Buttons 0-3 for CD32 (Blue, Green, Yellow, Shoulder L)
				if (btn & (1<<0)) jmap |= (1<<4);  // Button 0 -> Bit 4 (Blue)
				if (btn & (1<<1)) jmap |= (1<<5);  // Button 1 -> Bit 5 (Green)
				if (btn & (1<<2)) jmap |= (1<<6);  // Button 2 -> Bit 6 (Yellow)
				if (btn & (1<<3)) jmap |= (1<<7);  // Button 3 -> Bit 7 (Shoulder L)
				
				gamepad_info.gamepad_data = jmap;
				
				// Store all available extra buttons in extraBtn (8 bits available)
				// Buttons 4-6 from btn -> extraBtn bits 0-2
				// Buttons 7-11 from btn_extra -> extraBtn bits 3-7
				gamepad_info.gamepad_extraBtn = 0;  // Clear first
				if (btn & (1<<4)) gamepad_info.gamepad_extraBtn |= (1<<0);  // Button 4
				if (btn & (1<<5)) gamepad_info.gamepad_extraBtn |= (1<<1);  // Button 5
				if (btn & (1<<6)) gamepad_info.gamepad_extraBtn |= (1<<2);  // Button 6
				// Add btn_extra buttons (7-11) to bits 3-7
				if (btn_extra & (1<<0)) gamepad_info.gamepad_extraBtn |= (1<<3);  // Button 7
				if (btn_extra & (1<<1)) gamepad_info.gamepad_extraBtn |= (1<<4);  // Button 8
				if (btn_extra & (1<<2)) gamepad_info.gamepad_extraBtn |= (1<<5);  // Button 9
				if (btn_extra & (1<<3)) gamepad_info.gamepad_extraBtn |= (1<<6);  // Button 10
				if (btn_extra & (1<<4)) gamepad_info.gamepad_extraBtn |= (1<<7);  // Button 11

		  return USB_OK;
	    }



	  return USB_FAIL;

}

