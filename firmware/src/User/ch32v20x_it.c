/********************************** (C) COPYRIGHT *******************************
 * File Name          : ch32v20x_it.c
 * Author             : WCH
 * Version            : V1.0.0
 * Date               : 2021/06/06
 * Description        : Main Interrupt Service Routines.
 *********************************************************************************
 * Copyright (c) 2021 Nanjing Qinheng Microelectronics Co., Ltd.
 * Attention: This software (modified or not) and binary are used for
 * microcontroller manufactured by Nanjing Qinheng Microelectronics.
 *******************************************************************************/
#include "ch32v20x_it.h"
#include "mouse.h"
#include "gpio.h"
#include "cd32gamepad.h"
#include "gamepad.h"


void NMI_Handler (void) __attribute__ ((interrupt ("WCH-Interrupt-fast")));
void HardFault_Handler (void) __attribute__ ((interrupt ("WCH-Interrupt-fast")));
void EXTI15_10_IRQHandler (void) __attribute__ ((interrupt ("WCH-Interrupt-fast")));

/*********************************************************************
 * @fn      NMI_Handler
 *
 * @brief   This function handles NMI exception.
 *
 * @return  none
 */
void NMI_Handler (void) {
}

/*********************************************************************
 * @fn      HardFault_Handler
 *
 * @brief   This function handles Hard Fault exception.
 *
 * @return  none
 */
void HardFault_Handler (void) {
    while (1) {
    }
}

void EXTI15_10_IRQHandler (void) {
    // Check if gamepad is connected to determine which handler to use
    if (IsGamepadConnected()) {
        // Always call CD32 handler - it will handle both detection and protocol
        CD32Gamepad_HandleInterrupt();
    } else {
        // Mouse scroll button handler - only process on rising edge
        if (EXTI_GetITStatus(EXTI_Line10) != RESET) {
            // Check if this is a rising edge (pin is now high)
            if (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_10)) {
                ProcessScrollIRQ();
            }
            EXTI_ClearITPendingBit(EXTI_Line10);
        }
    }
}