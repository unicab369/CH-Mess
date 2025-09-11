#include "rv003usb.h"

uint8_t USB_SEND_FLAG = 0;

void usb_handle_user_data(
	struct usb_endpoint* e, int current_endpoint,
	uint8_t* data, int len, struct rv003usb_internal* ist
) {
	if (len > 0) {
		LogUEvent(1139, data[0], 0, current_endpoint);
	}
}

// Keyboard (8 bytes)
static uint8_t tsajoystick[8] = { 0x00 };

void usb_setKey(uint8_t key) {
    tsajoystick[4] = key;      // 0x05 = "b"; 0x53 = NUMLOCK; 0x39 = CAPSLOCK;
}

void usb_handle_user_in_request(
	struct usb_endpoint* e, uint8_t* scratchpad, int endp,
	uint32_t sendtok, struct rv003usb_internal* ist
) {
	if( endp == 1 ) {
        // if (USB_SEND_FLAG == 0) return;
        // USB_SEND_FLAG = 0;

		// // Mouse (4 bytes)
		// static int i;
		// static uint8_t tsajoystick[4] = { 0x00, 0x00, 0x00, 0x00 };
		// i++;
		// int mode = i >> 5;

		// // Move the mouse right, down, left and up in a square.
		// switch( mode & 3 ) {
		// 	case 0: tsajoystick[1] =  1; tsajoystick[2] = 0; break;
		// 	case 1: tsajoystick[1] =  0; tsajoystick[2] = 1; break;
		// 	case 2: tsajoystick[1] = -1; tsajoystick[2] = 0; break;
		// 	case 3: tsajoystick[1] =  0; tsajoystick[2] =-1; break;
		// }
		// usb_send_data( tsajoystick, 4, 0, sendtok );
	}
	else if( endp == 2 ) {
		usb_send_data( tsajoystick, 8, 0, sendtok );
        usb_setKey(0x00);
	}
	else {
		// If it's a control transfer, empty it.
		usb_send_empty( sendtok );
	}
}

