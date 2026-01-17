/*
 * cd32gamepad.h
 * 
 * CD32 gamepad protocol implementation
 * Handles serial communication for Commodore CD32 gamepad interface
 */

#ifndef __CD32GAMEPAD_H
#define __CD32GAMEPAD_H

#include <stdint.h>
#include "usb_gamepad.h"
#include "gpio.h"
#include "ch32v20x_exti.h"

// CD32 Button mapping (7 bits)
// Bit 0: Right
// Bit 1: Left  
// Bit 2: Down
// Bit 3: Up
// Bit 4: Button Blue (Fire 1)
// Bit 5: Button Red (Fire 2)
// Bit 6: Button Yellow (Fire 3)
// Additional buttons for extended CD32 pad:
// Bit 7: Button Green (Play/Pause)
// Bit 8: Button Forward/FFwd
// Bit 9: Button Reverse/Rewind
// Bit 10: Button Play/Start

// Initialize CD32 gamepad interface
void CD32Gamepad_Init(void);

// Update CD32 button data from USB gamepad
void CD32Gamepad_ProcessUSB(HID_gamepad_Info_TypeDef* gamepad);

// Get current CD32 button data (for interrupt handler)
uint8_t CD32Gamepad_GetButtonData(void);

// Get current button index (for interrupt handler)
uint8_t CD32Gamepad_GetButtonIndex(void);

// Reset button index to 0 (called on latch)
void CD32Gamepad_ResetIndex(void);

// Increment button index (called on clock)
void CD32Gamepad_IncrementIndex(void);

// Handle EXTI interrupt for CD32 protocol
void CD32Gamepad_HandleInterrupt(void);

#endif // __CD32GAMEPAD_H
