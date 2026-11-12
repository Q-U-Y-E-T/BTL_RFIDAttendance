#include "feedback.h"
#include "main.h"

#define FEEDBACK_ON_MS  250U
#define FEEDBACK_OFF_MS 250U

static uint8_t busy;
static uint8_t output_on;
static uint8_t remaining;
static uint32_t last_change;

static void set_outputs(uint8_t on)
{
    /* LED ngoai PA8 gia lap coi: High = bat. */
    HAL_GPIO_WritePin(
        BUZZER_GPIO_Port,
        BUZZER_Pin,
        on ? GPIO_PIN_SET : GPIO_PIN_RESET
    );

    /* LED PC13 tren Blue Pill: Low = sang. */
    HAL_GPIO_WritePin(
        LED_GPIO_Port,
        LED_Pin,
        on ? GPIO_PIN_RESET : GPIO_PIN_SET
    );
}

void Feedback_Init(void)
{
    busy = 0;
    output_on = 0;
    remaining = 0;
    last_change = HAL_GetTick();

    set_outputs(0);
}

uint8_t Feedback_IsBusy(void)
{
    return busy;
}

uint8_t Feedback_Start(AppStatus_t status)
{
    uint8_t pulses;

    if (busy) {
        return 0;
    }

    switch (status) {
    case APP_CARD_ACCEPTED:
        pulses = 1;
        break;

    case APP_CARD_UNKNOWN:
    case APP_STORAGE_ERROR:
    case APP_LIST_ERROR:
        pulses = 3;
        break;

    default:
        return 0;
    }

    remaining = pulses;
    output_on = 1;
    busy = 1;
    last_change = HAL_GetTick();

    set_outputs(1);
    return 1;
}

void Feedback_Process(void)
{
    if (!busy) {
        return;
    }

    uint32_t now = HAL_GetTick();
    uint32_t duration =
        output_on ? FEEDBACK_ON_MS : FEEDBACK_OFF_MS;

    if ((uint32_t)(now - last_change) < duration) {
        return;
    }

    last_change = now;

    if (output_on) {
        set_outputs(0);
        output_on = 0;

        --remaining;

        if (remaining == 0) {
            busy = 0;
        }
    } else {
        set_outputs(1);
        output_on = 1;
    }
}
