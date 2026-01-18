/*
 * cd32gamepad.c
 *
 * CD32 gamepad protocol implementation
 * Handles serial communication for Commodore CD32 gamepad interface
 */

#include "cd32gamepad.h"
#include "ch32v20x.h"

// Internal state variables
static volatile uint8_t cd32ButtonIndex = 0;
static volatile uint8_t cd32ButtonData = 0;
static volatile uint8_t cd32ButtonDataLatched = 0;  // Latched data for shifting out

/*
 * CD32 Protocol:
 * The CD32 gamepad uses a serial protocol with LATCH and CLOCK signals
 * - LATCH (Pin 10): Resets the shift register, starts new read cycle
 * - CLOCK (Pin 15): Shifts out the next bit on rising edge
 * - DATA (Pin 11): Output pin that sends button state (active low)
 *
 * Button data is sent as 7 bits in this order:
kdfkljsdf
 * Bit 4: Button Blue (Fire 1)
 * Bit 5: Button Red (Fire 2)
 * Bit 6: Button Yellow (Fire 3)
 *
 * Button pressed = 1 in the data byte
 * Output pin is active low (LOW = pressed, HIGH = not pressed)
 */

void CD32Gamepad_Init (void) {
    cd32ButtonIndex = 0;
    cd32ButtonData = 0;
    cd32ButtonDataLatched = 0;
}

void CD32Gamepad_ProcessUSB (HID_gamepad_Info_TypeDef *gamepad) {
    if (gamepad == NULL) {
        return;
    }

    uint8_t buttons = 0;

    // Map USB gamepad data to CD32 format
    // USB gamepad_data bits:
    // Bit 0-3: Directional (RIGHT, LEFT, DOWN, UP)
    // Bit 4: USB Button 0
    // Bit 5: USB Button 1
    // Bit 6: USB Button 2
    // Bit 7: USB Button 3
    // gamepad_extraBtn bits 0-2: USB Buttons 4-6

    // CD32 button order (7 bits total):
    // Bit 0: Red (Fire 1 / Start)
    // Bit 1: Blue (Fire 2 / Cancel)
    // Bit 2: Green
    // Bit 3: Yellow
    // Bit 4: Shoulder Left (L)
    // Bit 5: Shoulder Right (R)
    // Bit 6: Play/Pause

    // Map all 7 CD32 buttons
    if (gamepad->gamepad_data & (1 << 4))
        buttons |= (1 << 1);  // USB Button 0 -> Red (Fire 1 / Start)
    if (gamepad->gamepad_data & (1 << 5))
        buttons |= (1 << 6);  // USB Button 1 -> Blue (Fire 2 / Cancel)
    if (gamepad->gamepad_data & (1 << 6))
        buttons |= (1 << 0);  // USB Button 2 -> Green
    if (gamepad->gamepad_data & (1 << 7))
        buttons |= (1 << 2);  // USB Button 3 -> Yellow

    // Map extra buttons from gamepad_extraBtn
    if (gamepad->gamepad_extraBtn & (1 << 0))
        buttons |= (1 << 4);  // Shoulder Left (L) OK
    if (gamepad->gamepad_extraBtn & (1 << 1))
        buttons |= (1 << 3);  // Shoulder Right (R) ok
    if (gamepad->gamepad_extraBtn & ((1 << 4) | (1 << 5)))
        buttons |= (1 << 5);  // Play/Pause
    // 1,3,4,5,6 -

    // Output directional pins directly
    GPIO_WriteBit (RHQ_GPIO_Port, RHQ_Pin, !(gamepad->gamepad_data & 0x1));
    GPIO_WriteBit (LVQ_GPIO_Port, LVQ_Pin, !(gamepad->gamepad_data >> 1 & 0x1));
    GPIO_WriteBit (BH_GPIO_Port, BH_Pin, !(gamepad->gamepad_data >> 2 & 0x1));
    GPIO_WriteBit (FV_GPIO_Port, FV_Pin, !(gamepad->gamepad_data >> 3 & 0x1));

    // Atomic update: disable interrupts briefly to prevent race condition
    // with interrupt handler reading cd32ButtonData
    __disable_irq();
    cd32ButtonData = buttons;  // For testing
    __enable_irq();
}

uint8_t CD32Gamepad_GetButtonIndex (void) {
    return cd32ButtonIndex;
}

void CD32Gamepad_ResetIndex (void) {
    cd32ButtonIndex = 0;
}

void CD32Gamepad_IncrementIndex (void) {
    cd32ButtonIndex++;
}

void CD32Gamepad_HandleInterrupt (void) {
    // Latch signal (EXTI_Line10) - Start of new read cycle
    if (EXTI_GetITStatus (EXTI_Line10) != RESET) {
        // Latch the current button data - this freezes the state for shifting
        cd32ButtonDataLatched = cd32ButtonData;
        CD32Gamepad_ResetIndex();
        EXTI_ClearITPendingBit (EXTI_Line10);
    }

    // Clock signal (EXTI_Line15) - Shift out next bit
    if (EXTI_GetITStatus (EXTI_Line15) != RESET) {
        uint8_t index = CD32Gamepad_GetButtonIndex();

        // CD32 protocol sends 7 bits (0-6)
        if (index <= 6) {
            // Use the LATCHED data, not the live data
            // Check if current bit is set (button pressed)
            if (cd32ButtonDataLatched & (1 << index)) {
                // Button is pressed - output LOW (active low)
                GPIO_WriteBit (RB_GPIO_Port, RB_Pin, Bit_RESET);
            } else {
                // Button is not pressed - output HIGH
                GPIO_WriteBit (RB_GPIO_Port, RB_Pin, Bit_SET);
            }

            CD32Gamepad_IncrementIndex();
        }

        EXTI_ClearITPendingBit (EXTI_Line15);
    }
}
