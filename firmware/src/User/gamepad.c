#include "gamepad.h"
#include "usb_host_config.h"
#include "cd32gamepad.h"

uint8_t IsGamepadConnected(void)
{
    // Check all devices
    for (uint8_t device = 0; device < DEF_TOTAL_ROOT_HUB * DEF_ONE_USB_SUP_DEV_TOTAL; device++) {
        // Check all interfaces for this device
        for (uint8_t itf = 0; itf < HostCtl[device].InterfaceNum; itf++) {
            if (HostCtl[device].Interface[itf].HIDRptDesc.type == REPORT_TYPE_JOYSTICK) {
                return 1;
            }
        }
    }
    return 0;
}

void ProcessGamepad(HID_gamepad_Info_TypeDef* joymap)
{
    if (joymap == NULL) return;

    // Check CD32 mode status
    uint8_t cd32Mode = CD32Gamepad_IsCD32ModeActive();
    
    // Always output directional pins
    GPIO_WriteBit(RHQ_GPIO_Port, RHQ_Pin, !(joymap->gamepad_data & 0x1));
    GPIO_WriteBit(LVQ_GPIO_Port, LVQ_Pin, !(joymap->gamepad_data >> 1 & 0x1));
    GPIO_WriteBit(BH_GPIO_Port, BH_Pin, !(joymap->gamepad_data >> 2 & 0x1));
    GPIO_WriteBit(FV_GPIO_Port, FV_Pin, !(joymap->gamepad_data >> 3 & 0x1));
    
    // LB_Pin, MB_Pin and RB_Pin handling depends on CD32 mode
    if (!cd32Mode) {
        // Normal joystick mode - LB, MB and RB are output buttons
        GPIO_WriteBit(LB_GPIO_Port, LB_Pin, !(joymap->gamepad_data >> 4 & 0x1));
        GPIO_WriteBit(MB_GPIO_Port, MB_Pin, !(joymap->gamepad_data >> 5 & 0x1));
        GPIO_WriteBit(RB_GPIO_Port, RB_Pin, !(joymap->gamepad_data >> 6 & 0x1));
    }
    // In CD32 mode, LB is input (clock), MB is input (latch), RB is output (data), handled by CD32 protocol
}
