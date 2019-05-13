/* mbed Microcontroller Library
 * (C)Copyright TOSHIBA ELECTRONIC DEVICES & STORAGE CORPORATION 2017 All rights reserved
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "gpio_irq_api.h"
#include "mbed_error.h"
#include "PeripheralNames.h"
#include "pinmap.h"
#include "mbed_critical.h"

#define CHANNEL_NUM        6

const PinMap PinMap_GPIO_IRQ[] = {
    {PD5, GPIO_IRQ_0, PIN_DATA(0, 0)},
    {PA5, GPIO_IRQ_1, PIN_DATA(0, 0)},
    {PA6, GPIO_IRQ_2, PIN_DATA(0, 0)},
    {PF1, GPIO_IRQ_3, PIN_DATA(0, 0)},
    {PC5, GPIO_IRQ_4, PIN_DATA(0, 0)},
    {PF0, GPIO_IRQ_5, PIN_DATA(0, 0)},
    {NC,  NC,         0}
};

static uint32_t channel_ids[CHANNEL_NUM] = {0};
static gpio_irq_handler hal_irq_handler[CHANNEL_NUM] = {NULL};

static void INT_IRQHandler(PinName pin, GPIO_IRQName irq_id, uint32_t index)
{
    uint32_t val;
    GPIO_Port port;
    uint32_t mask;
    INTIFAO_INTActiveState ActiveState;
    port = (GPIO_Port)(pin >> 3);
    mask = 0x01 << (pin & 0x07);
    // Clear interrupt request
    INTIFAO_ClearINTReq((INTIFAO_INTSrc)(INTIFAO_INT_SRC_0 + index));
    // Clear gpio pending interrupt
    NVIC_ClearPendingIRQ((IRQn_Type)irq_id);
    ActiveState = INTIFAO_GetSTBYReleaseINTState((INTIFAO_INTSrc)(INTIFAO_INT_SRC_0 + index));
    INTIFAO_SetSTBYReleaseINTSrc((INTIFAO_INTSrc)(INTIFAO_INT_SRC_0 + index),
                                 ActiveState, DISABLE);
    // Get pin value
    val = GPIO_ReadDataBit(port, mask);
    switch (val) {
        // Falling edge detection
        case 0:
            hal_irq_handler[index](channel_ids[index], IRQ_FALL);
            break;
        // Rising edge detection
        case 1:
            hal_irq_handler[index](channel_ids[index], IRQ_RISE);
            break;
        default:
            break;
    }

    // Enable interrupt request
    INTIFAO_SetSTBYReleaseINTSrc((INTIFAO_INTSrc)(INTIFAO_INT_SRC_0 + index),
                                 ActiveState, ENABLE);
}

void INT0_IRQHandler(void)
{
    INT_IRQHandler(PD5, GPIO_IRQ_0, 0);
}

void INT1_IRQHandler(void)
{
    INT_IRQHandler(PA5, GPIO_IRQ_1, 1);
}

void INT2_IRQHandler(void)
{
    INT_IRQHandler(PA6, GPIO_IRQ_2, 2);
}

void INT3_IRQHandler(void)
{
    INT_IRQHandler(PF1, GPIO_IRQ_3, 3);
}

void INT4_IRQHandler(void)
{
    INT_IRQHandler(PC5, GPIO_IRQ_4, 4);
}

void INT5_IRQHandler(void)
{
    INT_IRQHandler(PF0, GPIO_IRQ_5, 5);
}

int gpio_irq_init(gpio_irq_t *obj, PinName pin, gpio_irq_handler handler, uint32_t id)
{
    // Get gpio interrupt ID
    obj->irq_id = pinmap_peripheral(pin, PinMap_GPIO_IRQ);
    core_util_critical_section_enter();
    // Get pin mask
    obj->mask = (uint32_t)(1 << (pin & 0x07));
    // Get GPIO port
    obj->port = (GPIO_Port)(pin >> 3);
    // Set pin level as LOW
    GPIO_WriteDataBit(obj->port, obj->mask, 0);
    // Enable gpio interrupt function
    pinmap_pinout(pin, PinMap_GPIO_IRQ);

    // Get GPIO irq source
    switch (obj->irq_id) {
        case GPIO_IRQ_0:
            obj->irq_src = INTIFAO_INT_SRC_0;
            break;
        case GPIO_IRQ_1:
            obj->irq_src = INTIFAO_INT_SRC_1;
            break;
        case GPIO_IRQ_2:
            obj->irq_src = INTIFAO_INT_SRC_2;
            break;
        case GPIO_IRQ_3:
            obj->irq_src = INTIFAO_INT_SRC_3;
            break;
        case GPIO_IRQ_4:
            obj->irq_src = INTIFAO_INT_SRC_4;
            break;
        case GPIO_IRQ_5:
            obj->irq_src = INTIFAO_INT_SRC_5;
            break;
        default:
            break;
    }

    // Save irq handler
    hal_irq_handler[obj->irq_src] = handler;
    // Save irq id
    channel_ids[obj->irq_src] = id;
    // Initialize interrupt event as both edges detection
    obj->event = INTIFAO_INT_ACTIVE_STATE_INVALID;
    // Set interrupt event and enable INTx clear
    INTIFAO_SetSTBYReleaseINTSrc(obj->irq_src, (INTIFAO_INTActiveState)obj->event, ENABLE);
    // Clear gpio pending interrupt
    NVIC_ClearPendingIRQ((IRQn_Type)obj->irq_id);
    core_util_critical_section_exit();;

    return 0;
}

void gpio_irq_free(gpio_irq_t *obj)
{
    // Clear gpio_irq
    NVIC_ClearPendingIRQ((IRQn_Type)obj->irq_id);
    // Reset interrupt handler
    hal_irq_handler[obj->irq_src] = NULL;
    // Reset interrupt id
    channel_ids[obj->irq_src] = 0;
}

void gpio_irq_set(gpio_irq_t *obj, gpio_irq_event event, uint32_t enable)
{
    //Disable GPIO interrupt on obj
    gpio_irq_disable(obj);
    if (enable) {
        // Get gpio interrupt event
        if (event == IRQ_RISE) {
            if ((obj->event == INTIFAO_INT_ACTIVE_STATE_FALLING) ||
                    (obj->event == INTIFAO_INT_ACTIVE_STATE_BOTH_EDGES)) {
                obj->event = INTIFAO_INT_ACTIVE_STATE_BOTH_EDGES;
            } else {
                obj->event = INTIFAO_INT_ACTIVE_STATE_RISING;
            }
        } else if (event == IRQ_FALL) {
            if ((obj->event == INTIFAO_INT_ACTIVE_STATE_RISING) ||
                    (obj->event == INTIFAO_INT_ACTIVE_STATE_BOTH_EDGES)) {
                obj->event = INTIFAO_INT_ACTIVE_STATE_BOTH_EDGES;
            } else {
                obj->event = INTIFAO_INT_ACTIVE_STATE_FALLING;
            }
        } else {
            error("Not supported event\n");
        }
    } else {
        // Get gpio interrupt event
        if (event == IRQ_RISE) {
            if ((obj->event == INTIFAO_INT_ACTIVE_STATE_RISING) ||
                    (obj->event == INTIFAO_INT_ACTIVE_STATE_INVALID)) {
                obj->event = INTIFAO_INT_ACTIVE_STATE_INVALID;
            } else {
                obj->event = INTIFAO_INT_ACTIVE_STATE_FALLING;
            }
        } else if (event == IRQ_FALL) {
            if ((obj->event == INTIFAO_INT_ACTIVE_STATE_FALLING) ||
                    (obj->event == INTIFAO_INT_ACTIVE_STATE_INVALID)) {
                obj->event = INTIFAO_INT_ACTIVE_STATE_INVALID;
            } else {
                obj->event = INTIFAO_INT_ACTIVE_STATE_RISING;
            }
        } else {
            error("Not supported event\n");
        }
    }

    if (obj->event != INTIFAO_INT_ACTIVE_STATE_INVALID) {
        // Set interrupt event and enable INTx clear
        INTIFAO_SetSTBYReleaseINTSrc(obj->irq_src, (INTIFAO_INTActiveState)obj->event, ENABLE);
        GPIO_SetOutputEnableReg(obj->port, obj->mask, DISABLE);
    } else {
        GPIO_SetOutputEnableReg(obj->port, obj->mask, ENABLE);
    }

    // Clear interrupt request
    INTIFAO_ClearINTReq(obj->irq_src);
    // Enable GPIO interrupt on obj
    gpio_irq_enable(obj);
}

void gpio_irq_enable(gpio_irq_t *obj)
{
    // Clear and Enable gpio_irq object
    NVIC_ClearPendingIRQ((IRQn_Type)obj->irq_id);
    NVIC_EnableIRQ((IRQn_Type)obj->irq_id);
}

void gpio_irq_disable(gpio_irq_t *obj)
{
    // Disable gpio_irq object
    NVIC_DisableIRQ((IRQn_Type)obj->irq_id);
}
