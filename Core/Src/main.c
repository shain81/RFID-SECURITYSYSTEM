/* =================================================================
 * سیستم امنیتی RFID با STM32F401xE
 *
 * اجزا:
 * - LCD LM044L (16x2)
 * - Keypad 4x4 (Calculator)
 * - 3 LEDs (1 سبز، 2 قرمز)
 * - Buzzer
 * - LOGICSTATE (PIR جایگزین)
 * - 3 Switch (RFID جایگزین)
 *
 * پین‌های استفاده شده:
 * PA0-PA1: LCD Control
 * PA4-PA7: LCD Data
 * PA8-PA11: LEDs + Buzzer
 * PB0-PB3: RFID + PIR
 * PC0-PC7: Keypad
 * ================================================================= */

#include "main.h"
#include <string.h>
#include "../../../PROJECT/LIB_LAB4_LCD/lcd.c"
/* تعریف پین‌های LCD */
#define LCD_RS_Pin GPIO_PIN_0
#define LCD_RS_GPIO_Port GPIOA
#define LCD_EN_Pin GPIO_PIN_1
#define LCD_EN_GPIO_Port GPIOA
#define LCD_D4_Pin GPIO_PIN_4
#define LCD_D4_GPIO_Port GPIOA
#define LCD_D5_Pin GPIO_PIN_5
#define LCD_D5_GPIO_Port GPIOA
#define LCD_D6_Pin GPIO_PIN_6
#define LCD_D6_GPIO_Port GPIOA
#define LCD_D7_Pin GPIO_PIN_7
#define LCD_D7_GPIO_Port GPIOA

/* تعریف پین‌های LED و Buzzer */
#define LED_GREEN_Pin GPIO_PIN_8
#define LED_GREEN_GPIO_Port GPIOA
#define LED_RED1_Pin GPIO_PIN_9
#define LED_RED1_GPIO_Port GPIOA
#define LED_RED2_Pin GPIO_PIN_10
#define LED_RED2_GPIO_Port GPIOA
#define BUZZER_Pin GPIO_PIN_11
#define BUZZER_GPIO_Port GPIOA

/* تعریف پین‌های سنسورها */
#define RFID_CARD1_Pin GPIO_PIN_0
#define RFID_CARD1_GPIO_Port GPIOB
#define RFID_CARD2_Pin GPIO_PIN_1
#define RFID_CARD2_GPIO_Port GPIOB
#define PIR_SENSOR_Pin GPIO_PIN_2
#define PIR_SENSOR_GPIO_Port GPIOB
#define RFID_CARD3_Pin GPIO_PIN_3
#define RFID_CARD3_GPIO_Port GPIOB

/* تعریف پین‌های کیپد */
#define KEYPAD_ROW1_Pin GPIO_PIN_0
#define KEYPAD_ROW1_GPIO_Port GPIOC
#define KEYPAD_ROW2_Pin GPIO_PIN_1
#define KEYPAD_ROW2_GPIO_Port GPIOC
#define KEYPAD_ROW3_Pin GPIO_PIN_2
#define KEYPAD_ROW3_GPIO_Port GPIOC
#define KEYPAD_ROW4_Pin GPIO_PIN_3
#define KEYPAD_ROW4_GPIO_Port GPIOC
#define KEYPAD_COL1_Pin GPIO_PIN_4
#define KEYPAD_COL1_GPIO_Port GPIOC
#define KEYPAD_COL2_Pin GPIO_PIN_5
#define KEYPAD_COL2_GPIO_Port GPIOC
#define KEYPAD_COL3_Pin GPIO_PIN_6
#define KEYPAD_COL3_GPIO_Port GPIOC
#define KEYPAD_COL4_Pin GPIO_PIN_7
#define KEYPAD_COL4_GPIO_Port GPIOC

/* متغیرهای سیستم */
typedef enum {
    SYSTEM_ARMED,
    SYSTEM_DISARMED,
    SYSTEM_ALARM,
    SYSTEM_PASSWORD_ENTRY
} SystemState_t;

SystemState_t currentState = SYSTEM_DISARMED;
char enteredPassword[10] = {0};
char correctPassword[] = "1234";
uint8_t passwordIndex = 0;
uint32_t alarmStartTime = 0;
uint8_t motionDetected = 0;

/* کیپد Calculator layout */
char keypadLayout[4][4] = {
    {'1', '2', '3', '+'},
    {'4', '5', '6', '-'},
    {'7', '8', '9', '*'},
    {'C', '0', '=', '/'}
};

/* اعلان توابع */
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
void Error_Handler(void);
void LCD_Init(void);
void LCD_SendCommand(uint8_t cmd);
void LCD_SendData(uint8_t data);
void LCD_Print(char* str);
void LCD_SetCursor(uint8_t row, uint8_t col);
void LCD_Clear(void);
char Keypad_GetKey(void);
void Security_CheckSensors(void);
void Security_ProcessPassword(char key);
void Security_SetState(SystemState_t newState);
void Security_HandleAlarm(void);
void Sound_Beep(uint16_t duration);
void LED_Control(uint8_t green, uint8_t red1, uint8_t red2);

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    LCD_Init();

    /* پیام خوش‌آمدگویی */
    LCD_Clear();
    LCD_Print("RFID Security");
    LCD_SetCursor(1, 0);
    LCD_Print("System Ready");
    HAL_Delay(1000);  // کاهش از 2000 به 1000

    /* شروع سیستم در حالت غیرفعال */
    Security_SetState(SYSTEM_DISARMED);

    while (1)
    {
        /* خواندن کیپد */
        char key = Keypad_GetKey();
        if (key != 0) {
            Security_ProcessPassword(key);
        }

        /* بررسی سنسورها */
        Security_CheckSensors();

        /* مدیریت آلارم */
        if (currentState == SYSTEM_ALARM) {
            Security_HandleAlarm();
        }

        HAL_Delay(50);  // کاهش از 100 به 50
    }
}

/* ================================================
 * توابع LCD
 * ================================================ */
void LCD_Init(void)
{
    HAL_Delay(20);  // delay ضروری برای power-up

    /* مراحل اولیه initialization */
    HAL_GPIO_WritePin(LCD_RS_GPIO_Port, LCD_RS_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LCD_EN_GPIO_Port, LCD_EN_Pin, GPIO_PIN_RESET);

    /* ارسال 0x03 سه بار برای تضمین 4-bit mode */
    for(int i = 0; i < 3; i++) {
        HAL_GPIO_WritePin(LCD_D4_GPIO_Port, LCD_D4_Pin, GPIO_PIN_SET);
        HAL_GPIO_WritePin(LCD_D5_GPIO_Port, LCD_D5_Pin, GPIO_PIN_SET);
        HAL_GPIO_WritePin(LCD_D6_GPIO_Port, LCD_D6_Pin, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(LCD_D7_GPIO_Port, LCD_D7_Pin, GPIO_PIN_RESET);

        HAL_GPIO_WritePin(LCD_EN_GPIO_Port, LCD_EN_Pin, GPIO_PIN_SET);
        HAL_Delay(1);
        HAL_GPIO_WritePin(LCD_EN_GPIO_Port, LCD_EN_Pin, GPIO_PIN_RESET);
        HAL_Delay(5);
    }

    /* تنظیم 4-bit mode */
    HAL_GPIO_WritePin(LCD_D4_GPIO_Port, LCD_D4_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LCD_D5_GPIO_Port, LCD_D5_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(LCD_D6_GPIO_Port, LCD_D6_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LCD_D7_GPIO_Port, LCD_D7_Pin, GPIO_PIN_RESET);

    HAL_GPIO_WritePin(LCD_EN_GPIO_Port, LCD_EN_Pin, GPIO_PIN_SET);
    HAL_Delay(1);
    HAL_GPIO_WritePin(LCD_EN_GPIO_Port, LCD_EN_Pin, GPIO_PIN_RESET);
    HAL_Delay(5);

    /* حالا از توابع عادی استفاده کنیم */
    LCD_SendCommand(0x28); // 4-bit mode, 2 lines, 5x8 dots
    LCD_SendCommand(0x0C); // Display ON, Cursor OFF
    LCD_SendCommand(0x06); // Auto increment cursor
    LCD_SendCommand(0x01); // Clear display
    HAL_Delay(2);
}

void LCD_SendCommand(uint8_t cmd)
{
    /* RS = 0 برای دستور */
    HAL_GPIO_WritePin(LCD_RS_GPIO_Port, LCD_RS_Pin, GPIO_PIN_RESET);

    /* ارسال 4 بیت بالا */
    HAL_GPIO_WritePin(LCD_D4_GPIO_Port, LCD_D4_Pin, (cmd & 0x10) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LCD_D5_GPIO_Port, LCD_D5_Pin, (cmd & 0x20) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LCD_D6_GPIO_Port, LCD_D6_Pin, (cmd & 0x40) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LCD_D7_GPIO_Port, LCD_D7_Pin, (cmd & 0x80) ? GPIO_PIN_SET : GPIO_PIN_RESET);

    /* پالس Enable - حداقل delay برای عملکرد صحیح */
    HAL_GPIO_WritePin(LCD_EN_GPIO_Port, LCD_EN_Pin, GPIO_PIN_SET);
    for(volatile int i = 0; i < 100; i++); // delay کوتاه بدون HAL_Delay
    HAL_GPIO_WritePin(LCD_EN_GPIO_Port, LCD_EN_Pin, GPIO_PIN_RESET);
    for(volatile int i = 0; i < 100; i++); // delay کوتاه بدون HAL_Delay

    /* ارسال 4 بیت پایین */
    HAL_GPIO_WritePin(LCD_D4_GPIO_Port, LCD_D4_Pin, (cmd & 0x01) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LCD_D5_GPIO_Port, LCD_D5_Pin, (cmd & 0x02) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LCD_D6_GPIO_Port, LCD_D6_Pin, (cmd & 0x04) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LCD_D7_GPIO_Port, LCD_D7_Pin, (cmd & 0x08) ? GPIO_PIN_SET : GPIO_PIN_RESET);

    /* پالس Enable - حداقل delay برای عملکرد صحیح */
    HAL_GPIO_WritePin(LCD_EN_GPIO_Port, LCD_EN_Pin, GPIO_PIN_SET);
    for(volatile int i = 0; i < 100; i++); // delay کوتاه بدون HAL_Delay
    HAL_GPIO_WritePin(LCD_EN_GPIO_Port, LCD_EN_Pin, GPIO_PIN_RESET);
    for(volatile int i = 0; i < 400; i++); // delay کمی طولانی‌تر برای command
}

void LCD_SendData(uint8_t data)
{
    /* RS = 1 برای داده */
    HAL_GPIO_WritePin(LCD_RS_GPIO_Port, LCD_RS_Pin, GPIO_PIN_SET);

    /* ارسال 4 بیت بالا */
    HAL_GPIO_WritePin(LCD_D4_GPIO_Port, LCD_D4_Pin, (data & 0x10) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LCD_D5_GPIO_Port, LCD_D5_Pin, (data & 0x20) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LCD_D6_GPIO_Port, LCD_D6_Pin, (data & 0x40) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LCD_D7_GPIO_Port, LCD_D7_Pin, (data & 0x80) ? GPIO_PIN_SET : GPIO_PIN_RESET);

    /* پالس Enable - حداقل delay برای عملکرد صحیح */
    HAL_GPIO_WritePin(LCD_EN_GPIO_Port, LCD_EN_Pin, GPIO_PIN_SET);
    for(volatile int i = 0; i < 100; i++); // delay کوتاه بدون HAL_Delay
    HAL_GPIO_WritePin(LCD_EN_GPIO_Port, LCD_EN_Pin, GPIO_PIN_RESET);
    for(volatile int i = 0; i < 100; i++); // delay کوتاه بدون HAL_Delay

    /* ارسال 4 بیت پایین */
    HAL_GPIO_WritePin(LCD_D4_GPIO_Port, LCD_D4_Pin, (data & 0x01) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LCD_D5_GPIO_Port, LCD_D5_Pin, (data & 0x02) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LCD_D6_GPIO_Port, LCD_D6_Pin, (data & 0x04) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LCD_D7_GPIO_Port, LCD_D7_Pin, (data & 0x08) ? GPIO_PIN_SET : GPIO_PIN_RESET);

    /* پالس Enable - حداقل delay برای عملکرد صحیح */
    HAL_GPIO_WritePin(LCD_EN_GPIO_Port, LCD_EN_Pin, GPIO_PIN_SET);
    for(volatile int i = 0; i < 100; i++); // delay کوتاه بدون HAL_Delay
    HAL_GPIO_WritePin(LCD_EN_GPIO_Port, LCD_EN_Pin, GPIO_PIN_RESET);
    for(volatile int i = 0; i < 200; i++); // delay کمی طولانی‌تر برای data
}

void LCD_Print(char* str)
{
    while (*str) {
        LCD_SendData(*str++);
    }
}

void LCD_SetCursor(uint8_t row, uint8_t col)
{
    uint8_t address;
    if (row == 0)
        address = 0x80 + col;
    else
        address = 0xC0 + col;
    LCD_SendCommand(address);
}

void LCD_Clear(void)
{
    LCD_SendCommand(0x01);
    HAL_Delay(2);  // برگرداندن delay ضروری برای clear command
}

/* ================================================
 * توابع کیپد
 * ================================================ */
char Keypad_GetKey(void)
{
    uint16_t rows[4] = {KEYPAD_ROW1_Pin, KEYPAD_ROW2_Pin, KEYPAD_ROW3_Pin, KEYPAD_ROW4_Pin};
    uint16_t cols[4] = {KEYPAD_COL1_Pin, KEYPAD_COL2_Pin, KEYPAD_COL3_Pin, KEYPAD_COL4_Pin};

    for (int col = 0; col < 4; col++) {
        /* تنظیم همه ستون‌ها به HIGH */
        HAL_GPIO_WritePin(GPIOC, KEYPAD_COL1_Pin | KEYPAD_COL2_Pin | KEYPAD_COL3_Pin | KEYPAD_COL4_Pin, GPIO_PIN_SET);

        /* تنظیم ستون جاری به LOW */
        HAL_GPIO_WritePin(GPIOC, cols[col], GPIO_PIN_RESET);

        HAL_Delay(2);  // کاهش از 10 به 2

        /* خواندن ردیف‌ها */
        for (int row = 0; row < 4; row++) {
            if (HAL_GPIO_ReadPin(GPIOC, rows[row]) == GPIO_PIN_RESET) {
                /* منتظر رها شدن دکمه */
                while (HAL_GPIO_ReadPin(GPIOC, rows[row]) == GPIO_PIN_RESET);
                HAL_Delay(20); // کاهش از 50 به 20
                return keypadLayout[row][col];
            }
        }
    }
    return 0; // هیچ دکمه‌ای فشرده نشده
}

/* ================================================
 * توابع امنیتی
 * ================================================ */
void Security_CheckSensors(void)
{
    static uint8_t lastPirState = 0;

    /* بررسی RFID Cards */
    if (HAL_GPIO_ReadPin(RFID_CARD1_GPIO_Port, RFID_CARD1_Pin) == GPIO_PIN_RESET) {
        /* کارت مجاز 1 */
        if (currentState == SYSTEM_ARMED) {
            Security_SetState(SYSTEM_DISARMED);
            Sound_Beep(200);
        }
    }
    else if (HAL_GPIO_ReadPin(RFID_CARD2_GPIO_Port, RFID_CARD2_Pin) == GPIO_PIN_RESET) {
        /* کارت مجاز 2 */
        if (currentState == SYSTEM_ARMED) {
            Security_SetState(SYSTEM_DISARMED);
            Sound_Beep(200);
        }
    }
    else if (HAL_GPIO_ReadPin(RFID_CARD3_GPIO_Port, RFID_CARD3_Pin) == GPIO_PIN_RESET) {
        /* کارت غیرمجاز */
        if (currentState == SYSTEM_ARMED) {
            Security_SetState(SYSTEM_ALARM);
        }
    }

    /* بررسی PIR Sensor (LOGICSTATE) */
    uint8_t currentPirState = HAL_GPIO_ReadPin(PIR_SENSOR_GPIO_Port, PIR_SENSOR_Pin);
    if (currentPirState == GPIO_PIN_SET && lastPirState == GPIO_PIN_RESET) {
        /* تشخیص حرکت */
        motionDetected = 1;
        if (currentState == SYSTEM_ARMED) {
            Security_SetState(SYSTEM_ALARM);
        }
    }
    lastPirState = currentPirState;
}

/* تابع اصلاح شده Security_ProcessPassword */
void Security_ProcessPassword(char key)
{
    if (key == 'C') {
        /* پاک کردن رمز */
        memset(enteredPassword, 0, sizeof(enteredPassword));
        passwordIndex = 0;
        LCD_Clear();
        if (currentState == SYSTEM_PASSWORD_ENTRY) {
            LCD_Print("Enter Password:");
            LCD_SetCursor(1, 0);
        } else {
            Security_SetState(currentState); // بازگشت به وضعیت قبلی
        }
        return;
    }

    if (key == '=' || key == '\n') {
        /* تأیید رمز */
        if (strcmp(enteredPassword, correctPassword) == 0) {
            /* رمز صحیح */
            LCD_Clear();
            LCD_Print("Password OK!");
            LCD_SetCursor(1, 0);
            LCD_Print("Access Granted");

            /* LED سبز روشن کن برای نشان دادن موفقیت */
            LED_Control(1, 0, 0); // فقط LED سبز روشن
            Sound_Beep(200);  // بوق کوتاه موفقیت
            HAL_Delay(2000);  // انتظار برای نمایش پیام

            /* حالا تغییر وضعیت */
            if (currentState == SYSTEM_DISARMED || currentState == SYSTEM_PASSWORD_ENTRY) {
                Security_SetState(SYSTEM_ARMED);
            } else if (currentState == SYSTEM_ARMED || currentState == SYSTEM_ALARM) {
                Security_SetState(SYSTEM_DISARMED);
            }
        } else {
            /* رمز اشتباه */
            LCD_Clear();
            LCD_Print("Wrong Password!");
            LCD_SetCursor(1, 0);
            LCD_Print("Access Denied");

            /* هر دو LED قرمز روشن کن + چشمک */
            for(int i = 0; i < 3; i++) {
                LED_Control(0, 1, 1); // هر دو قرمز روشن
                HAL_Delay(300);
                LED_Control(0, 0, 0); // خاموش
                HAL_Delay(300);
            }
            Sound_Beep(500);  // بوق طولانی خطا

            /* بازگشت به وضعیت قبلی */
            Security_SetState(currentState);
        }

        /* پاک کردن رمز وارد شده */
        memset(enteredPassword, 0, sizeof(enteredPassword));
        passwordIndex = 0;
        return;
    }

    /* اگر عدد است */
    if (key >= '0' && key <= '9') {
        if (passwordIndex < sizeof(enteredPassword) - 1) {
            if (currentState != SYSTEM_PASSWORD_ENTRY) {
                Security_SetState(SYSTEM_PASSWORD_ENTRY);
            }
            enteredPassword[passwordIndex] = key;
            passwordIndex++;

            /* نمایش ستاره به جای عدد */
            LCD_SetCursor(1, passwordIndex - 1);
            LCD_SendData('*');

            /* اگر 4 رقم وارد شد، خودکار چک کن */
            if (passwordIndex == 4) {
                HAL_Delay(500);  // کمی صبر کن تا کاربر ببیند
                Security_ProcessPassword('=');  // خودکار چک کن
            }
        }
    }
}




void Security_SetState(SystemState_t newState)
{
    currentState = newState;
    LCD_Clear();

    switch (currentState) {
        case SYSTEM_DISARMED:
            LCD_Print("System DISARMED");
            LCD_SetCursor(1, 0);
            LCD_Print("Press *=* to ARM");
            LED_Control(1, 0, 0); // سبز روشن، قرمزها خاموش
            break;

        case SYSTEM_ARMED:
            LCD_Print("System ARMED");
            LCD_SetCursor(1, 0);
            LCD_Print("Monitoring...");
            LED_Control(0, 1, 0); // قرمز 1 (PA9) روشن، بقیه خاموش
            break;

        case SYSTEM_ALARM:
            LCD_Print("!! ALARM !!");
            LCD_SetCursor(1, 0);
            if (motionDetected) {
                LCD_Print("Motion Detected");
                motionDetected = 0;
            } else {
                LCD_Print("Unauthorized");
            }
            LED_Control(0, 1, 1); // هر دو قرمز روشن
            alarmStartTime = HAL_GetTick();
            break;

        case SYSTEM_PASSWORD_ENTRY:
            LCD_Print("Enter Password:");
            LCD_SetCursor(1, 0);
            LED_Control(0, 0, 1); // قرمز 2 (PA10) روشن برای ورود پسورد
            break;
    }
}


/* تابع تست LED برای بررسی اتصالات */
void Test_All_LEDs(void)
{
    /* تست LED سبز */
    LCD_Clear();
    LCD_Print("Testing Green");
    LED_Control(1, 0, 0);
    HAL_Delay(1000);

    /* تست LED قرمز 1 (PA9) */
    LCD_Clear();
    LCD_Print("Testing Red1-PA9");
    LED_Control(0, 1, 0);
    HAL_Delay(1000);

    /* تست LED قرمز 2 (PA10) */
    LCD_Clear();
    LCD_Print("Testing Red2-PA10");
    LED_Control(0, 0, 1);
    HAL_Delay(1000);

    /* تست همه با هم */
    LCD_Clear();
    LCD_Print("Testing All");
    LED_Control(1, 1, 1);
    HAL_Delay(1000);

    /* خاموش کردن همه */
    LED_Control(0, 0, 0);
}


/* اضافه کردن این تست به main برای بررسی LEDها */
void main_with_led_test(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    LCD_Init();

    /* تست LEDها در شروع */
    Test_All_LEDs();

    /* پیام خوش‌آمدگویی */
    LCD_Clear();
    LCD_Print("RFID Security");
    LCD_SetCursor(1, 0);
    LCD_Print("System Ready");
    HAL_Delay(1000);

    /* شروع سیستم در حالت غیرفعال */
    Security_SetState(SYSTEM_DISARMED);

    while (1)
    {
        /* خواندن کیپد */
        char key = Keypad_GetKey();
        if (key != 0) {
            Security_ProcessPassword(key);
        }

        /* بررسی سنسورها */
        Security_CheckSensors();

        /* مدیریت آلارم */
        if (currentState == SYSTEM_ALARM) {
            Security_HandleAlarm();
        }

        HAL_Delay(50);
    }
}


void Security_HandleAlarm(void)
{
    static uint32_t lastBeepTime = 0;
    uint32_t currentTime = HAL_GetTick();

    /* بوق زدن هر 300 میلی‌ثانیه */
    if (currentTime - lastBeepTime > 300) {  // کاهش از 500 به 300
        Sound_Beep(100);  // کاهش از 200 به 100
        lastBeepTime = currentTime;
    }

    /* چشمک زدن LEDs */
    static uint8_t ledState = 0;
    if (currentTime - alarmStartTime > 150) {  // کاهش از 250 به 150
        ledState = !ledState;
        LED_Control(0, ledState, !ledState);
        alarmStartTime = currentTime;
    }
}

/* ================================================
 * توابع صدا و LED
 * ================================================ */
void Sound_Beep(uint16_t duration)
{
    HAL_GPIO_WritePin(BUZZER_GPIO_Port, BUZZER_Pin, GPIO_PIN_SET);
    HAL_Delay(duration);
    HAL_GPIO_WritePin(BUZZER_GPIO_Port, BUZZER_Pin, GPIO_PIN_RESET);
}

void LED_Control(uint8_t green, uint8_t red1, uint8_t red2)
{
    HAL_GPIO_WritePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin, green ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED_RED1_GPIO_Port, LED_RED1_Pin, red1 ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED_RED2_GPIO_Port, LED_RED2_Pin, red2 ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

/* ================================================
 * تنظیمات سیستم - اصلاح شده برای STM32F401
 * ================================================ */
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    /** Configure the main internal regulator output voltage */
    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);

    /** Initializes the RCC Oscillators according to the specified parameters
     * in the RCC_OscInitTypeDef structure.
     */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM = 25;    // HSE/25 = 8MHz/25 (assuming 8MHz HSE)
    RCC_OscInitStruct.PLL.PLLN = 168;   // VCO = 8MHz * 168
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2; // PLLCLK = VCO/2 = 84MHz
    RCC_OscInitStruct.PLL.PLLQ = 4;     // USB Clock = VCO/4

    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
        Error_Handler();
    }

    /** Initializes the CPU, AHB and APB buses clocks */
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                                |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK) {
        Error_Handler();
    }
}

static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* فعال‌سازی کلاک GPIO */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    /* تنظیم پین‌های خروجی LCD */
    GPIO_InitStruct.Pin = LCD_RS_Pin|LCD_EN_Pin|LCD_D4_Pin|LCD_D5_Pin|LCD_D6_Pin|LCD_D7_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* تنظیم پین‌های خروجی LED و Buzzer */
    GPIO_InitStruct.Pin = LED_GREEN_Pin|LED_RED1_Pin|LED_RED2_Pin|BUZZER_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* تنظیم پین‌های ورودی سنسورها */
    GPIO_InitStruct.Pin = RFID_CARD1_Pin|RFID_CARD2_Pin|PIR_SENSOR_Pin|RFID_CARD3_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /* تنظیم پین‌های کیپد */
    /* ردیف‌ها - ورودی با Pull-up */
    GPIO_InitStruct.Pin = KEYPAD_ROW1_Pin|KEYPAD_ROW2_Pin|KEYPAD_ROW3_Pin|KEYPAD_ROW4_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    /* ستون‌ها - خروجی */
    GPIO_InitStruct.Pin = KEYPAD_COL1_Pin|KEYPAD_COL2_Pin|KEYPAD_COL3_Pin|KEYPAD_COL4_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    /* تنظیم حالت اولیه پین‌های خروجی */
    HAL_GPIO_WritePin(GPIOA, LED_GREEN_Pin|LED_RED1_Pin|LED_RED2_Pin|BUZZER_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOC, KEYPAD_COL1_Pin|KEYPAD_COL2_Pin|KEYPAD_COL3_Pin|KEYPAD_COL4_Pin, GPIO_PIN_SET);
}

void Error_Handler(void)
{
    __disable_irq();
    while (1) {
        /* LED قرمز چشمک بزند */
        HAL_GPIO_TogglePin(LED_RED1_GPIO_Port, LED_RED1_Pin);
        HAL_Delay(200);
    }
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
    /* کاربر می‌تواند اینجا کد debug اضافه کند */
}
#endif
