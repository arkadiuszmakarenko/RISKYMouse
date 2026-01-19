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
static volatile uint16_t cd32ButtonData = 0;
static volatile uint16_t cd32ButtonDataLatched = 0;  // Latched data for shifting out
static volatile uint8_t savedPinStates = 0;  // Store GPIO states during CD32 mode
static volatile uint8_t cd32ModeEnabled = 0;  // 0 = normal joystick mode, 1 = CD32 mode
static volatile uint32_t lastDetectionCheckTime = 0;  // Timestamp of last detection check
static volatile uint16_t pulseCounterMB = 0;  // Count MB pulses during detection window
static volatile uint16_t pulseCounterLB = 0;  // Count LB pulses during detection window
static volatile uint8_t detectionActive = 0;  // 1 during detection window
static volatile uint8_t detectionCompleted = 0;  // 1 after detection has run once
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
    cd32ModeEnabled = 0;  // Start in normal joystick mode
    lastDetectionCheckTime = 0;
    pulseCounterMB = 0;
    pulseCounterLB = 0;
    detectionActive = 0;
    detectionCompleted = 0;
}

void CD32Gamepad_ProcessUSB (HID_gamepad_Info_TypeDef *gamepad) {
    if (gamepad == NULL) {
        return;
    }

    uint16_t buttons = 0;


    // Map all 7 CD32 buttons
    if (gamepad->gamepad_data & (1 << 4))
        buttons |= (1 << 2);  // USB Button 0 -> Red (Fire 1 / Start)
    if (gamepad->gamepad_data & (1 << 5))
        buttons |= (1 << 0);  // USB Button 1 -> Blue (Fire 2 / Cancel)
    if (gamepad->gamepad_data & (1 << 6))
        buttons |= (1 << 1);  // USB Button 2 -> Green
    if (gamepad->gamepad_data & (1 << 7))
        buttons |= (1 << 3);  // USB Button 3 -> Yellow

    // Map extra buttons from gamepad_extraBtn
    if (gamepad->gamepad_extraBtn & (1 << 0))
        buttons |= (1 << 5);  // Shoulder Left (L) OK
    if (gamepad->gamepad_extraBtn & (1 << 1))
        buttons |= (1 << 4);  // Shoulder Right (R) ok
    if (gamepad->gamepad_extraBtn & ((1 << 4) | (1 << 5)))
        buttons |= (1 << 6);  // Play/Pause

    // gamepad detection pattern.
    buttons |= (1 << 8);
    buttons |= (1 << 9);


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
    // Latch signal (EXTI_Line10 / MB_Pin) - Start of new read cycle
    if (EXTI_GetITStatus (EXTI_Line10) != RESET) {
        // If in detection mode, just count MB pulses
        if (detectionActive) {
            pulseCounterMB++;
            EXTI_ClearITPendingBit (EXTI_Line10);
            return;
        }
        
        // Only process CD32 protocol if mode is enabled
        if (!cd32ModeEnabled) {
            EXTI_ClearITPendingBit (EXTI_Line10);
            return;
        }
        
        // Read current pin state to determine if falling or rising edge
        uint8_t pinState = GPIO_ReadInputDataBit (MB_GPIO_Port, MB_Pin);

        if (pinState == 0) {
            // FALLING EDGE - Latch data and save GPIO states
            cd32ButtonDataLatched = cd32ButtonData;
            CD32Gamepad_ResetIndex();

            // Save current GPIO output states for LB, RB, MB
            savedPinStates = 0;
            if (GPIO_ReadOutputDataBit (LB_GPIO_Port, LB_Pin))
                savedPinStates |= (1 << 0);
            if (GPIO_ReadOutputDataBit (RB_GPIO_Port, RB_Pin))
                savedPinStates |= (1 << 1);
            if (GPIO_ReadOutputDataBit (MB_GPIO_Port, MB_Pin))
                savedPinStates |= (1 << 2);

            // Output first bit (bit 0) immediately at latch
            if (cd32ButtonDataLatched & (1 << 0)) {
                GPIO_WriteBit (RB_GPIO_Port, RB_Pin, Bit_RESET);  // Button pressed - LOW
            } else {
                GPIO_WriteBit (RB_GPIO_Port, RB_Pin, Bit_SET);    // Button not pressed - HIGH
            }
        } else {
            // RISING EDGE - Restore GPIO states
            GPIO_WriteBit (LB_GPIO_Port, LB_Pin, (savedPinStates & (1 << 0)) ? Bit_SET : Bit_RESET);
            GPIO_WriteBit (RB_GPIO_Port, RB_Pin, (savedPinStates & (1 << 1)) ? Bit_SET : Bit_RESET);
            GPIO_WriteBit (MB_GPIO_Port, MB_Pin, (savedPinStates & (1 << 2)) ? Bit_SET : Bit_RESET);
        }

        EXTI_ClearITPendingBit (EXTI_Line10);
    }

    // Clock signal (EXTI_Line15 / LB_Pin) - Shift out next bit
    if (EXTI_GetITStatus (EXTI_Line15) != RESET) {
        // If in detection mode, just count LB clock pulses
        if (detectionActive) {
            pulseCounterLB++;
            EXTI_ClearITPendingBit (EXTI_Line15);
            return;
        }
        
        // Only process CD32 protocol if mode is enabled
        if (!cd32ModeEnabled) {
            EXTI_ClearITPendingBit (EXTI_Line15);
            return;
        }
        
        uint8_t index = CD32Gamepad_GetButtonIndex();

        // CD32 protocol sends more than 7 bits for autodetection
        if (index <= 9) {
            // Increment index first (bit 0 was already sent at latch)
            CD32Gamepad_IncrementIndex();
            index = CD32Gamepad_GetButtonIndex();

            // Use the LATCHED data, not the live data
            // Check if current bit is set (button pressed)
            if (cd32ButtonDataLatched & (1 << index)) {
                // Button is pressed - output LOW (active low)
                GPIO_WriteBit (RB_GPIO_Port, RB_Pin, Bit_RESET);
            } else {
                // Button is not pressed - output HIGH
                GPIO_WriteBit (RB_GPIO_Port, RB_Pin, Bit_SET);
            }
        }

        EXTI_ClearITPendingBit (EXTI_Line15);
    }
}

uint8_t CD32Gamepad_IsCD32ModeActive(void) {
    return cd32ModeEnabled;
}

void CD32Gamepad_EnableCD32Mode(void) {
    if (cd32ModeEnabled) return;  // Already enabled
    
    cd32ModeEnabled = 1;
    
    // Reconfigure MB_Pin as input with interrupt for CD32 latch signal
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    GPIO_InitStructure.GPIO_Pin = MB_Pin;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;  // Input with pull-up
    GPIO_Init(MB_GPIO_Port, &GPIO_InitStructure);
    
    // Reconfigure LB_Pin as input with interrupt for CD32 clock signal
    GPIO_InitStructure.GPIO_Pin = LB_Pin;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;  // Input with pull-up
    GPIO_Init(LB_GPIO_Port, &GPIO_InitStructure);
    
    // Enable EXTI interrupt for both MB_Pin and LB_Pin
    NVIC_EnableIRQ(EXTI15_10_IRQn);
}

void CD32Gamepad_DisableCD32Mode(void) {
    if (!cd32ModeEnabled) return;  // Already disabled
    
    cd32ModeEnabled = 0;
    
    // Reconfigure MB_Pin as output for normal joystick button
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    GPIO_InitStructure.GPIO_Pin = MB_Pin;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(MB_GPIO_Port, &GPIO_InitStructure);
    
    // Set to high (button not pressed)
    GPIO_WriteBit(MB_GPIO_Port, MB_Pin, Bit_SET);
    
    // Reconfigure LB_Pin as output for normal joystick button
    GPIO_InitStructure.GPIO_Pin = LB_Pin;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(LB_GPIO_Port, &GPIO_InitStructure);
    
    // Set to high (button not pressed)
    GPIO_WriteBit(LB_GPIO_Port, LB_Pin, Bit_SET);
}

extern volatile uint32_t system_tick_ms;  // 1ms timer from TIM1

void CD32Gamepad_CheckForCD32Pulses(void) {
    // If detection already completed, don't run again
    if (detectionCompleted) {
        return;
    }
    
    uint32_t currentTime = system_tick_ms;
    
    // Wait at least 500ms after connection before checking
    if (currentTime - lastDetectionCheckTime < 500) {
        return;
    }
    
    lastDetectionCheckTime = currentTime;
    detectionCompleted = 1;  // Mark detection as done
    
    // Start detection window: configure MB and LB as inputs and start counting
    detectionActive = 1;
    pulseCounterMB = 0;
    pulseCounterLB = 0;
    
    // Temporarily configure MB_Pin and LB_Pin as inputs to detect pulses
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    
    // Configure MB as input (latch signal)
    GPIO_InitStructure.GPIO_Pin = MB_Pin;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;  // Input with pull-up
    GPIO_Init(MB_GPIO_Port, &GPIO_InitStructure);
    
    // Configure LB as input (clock signal)
    GPIO_InitStructure.GPIO_Pin = LB_Pin;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;  // Input with pull-up
    GPIO_Init(LB_GPIO_Port, &GPIO_InitStructure);
    
    // Enable EXTI interrupts temporarily for pulse counting
    NVIC_EnableIRQ(EXTI15_10_IRQn);
    
    // Wait 100ms to count pulses
    uint32_t detectionStart = system_tick_ms;
    while (system_tick_ms - detectionStart < 100) {
        // Just wait
    }
    
    detectionActive = 0;
    
    // Check if we detected CD32 pulses
    // CD32 requires BOTH MB (latch) and LB (clock) pulses
    // Threshold: at least 5 MB pulses AND at least 5 LB pulses in 100ms
    if (pulseCounterMB >= 5 && pulseCounterLB >= 5) {
        // CD32 protocol detected - enable CD32 mode
        CD32Gamepad_EnableCD32Mode();
    } else {
        // No CD32 detected - use normal joystick mode
        CD32Gamepad_DisableCD32Mode();
    }
}

void CD32Gamepad_ResetDetection(void) {
    // Reset detection state for new USB device
    detectionCompleted = 0;
    detectionActive = 0;
    pulseCounterMB = 0;
    pulseCounterLB = 0;
    lastDetectionCheckTime = system_tick_ms;
    
    // Disable CD32 mode and restore normal joystick pins
    if (cd32ModeEnabled) {
        CD32Gamepad_DisableCD32Mode();
    }
}
