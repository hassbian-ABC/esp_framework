#include "Common.h"
#include "Log.h"

#ifdef ESP32

EEPROMClass EEPROMconfig("espconfig", SPI_FLASH_SEC_SIZE);
bool isIniteeprom = false;
bool espconfig_spiflash_init()
{
    if (isIniteeprom)
    {
        return true;
    }
    if (!EEPROMconfig.begin(SPI_FLASH_SEC_SIZE))
    {
        Log::Error(PSTR("Failed to initialise EEPROMconfig"));
        delay(1000);
        ESP.restart();
        return false;
    }
    isIniteeprom = true;
    return true;
}

bool espconfig_spiflash_erase_sector(size_t sector)
{
    espconfig_spiflash_init();
    for (int i = 0; i < SPI_FLASH_SEC_SIZE; i++)
    {
        EEPROMconfig.write(i, 0xFF);
    }
    return EEPROMconfig.commit();
}

bool espconfig_spiflash_write(size_t dest_addr, const void *src, size_t size)
{
    espconfig_spiflash_init();
    if (EEPROMconfig.writeBytes(dest_addr, src, size) == 0)
    {
        return false;
    }
    return EEPROMconfig.commit();
}

bool espconfig_spiflash_read(size_t src_addr, void *dest, size_t size)
{
    espconfig_spiflash_init();
    if (EEPROMconfig.readBytes(src_addr, dest, size) == 0)
    {
        return false;
    }
    return true;
}

uint32_t sntp_get_current_timestamp()
{
    time_t now;
    time(&now);
    return now;
}

bool Ticker::active()
{
    return _timer;
}

uint32_t ESP_getChipId(void)
{
    uint32_t id = 0;
    for (uint32_t i = 0; i < 17; i = i + 8)
    {
        id |= ((ESP.getEfuseMac() >> (40 - i)) & 0xff) << i;
    }
    return id;
}

String ESP32GetResetReason(uint32_t cpu_no)
{
    // tools\sdk\include\esp32\rom\rtc.h
    switch (rtc_get_reset_reason((RESET_REASON)cpu_no))
    {
    case POWERON_RESET:
        return F("Vbat power on reset"); // 1
    case SW_RESET:
        return F("Software reset digital core"); // 3
    case OWDT_RESET:
        return F("Legacy watch dog reset digital core"); // 4
    case DEEPSLEEP_RESET:
        return F("Deep Sleep reset digital core"); // 5
    case SDIO_RESET:
        return F("Reset by SLC module, reset digital core"); // 6
    case TG0WDT_SYS_RESET:
        return F("Timer Group0 Watch dog reset digital core"); // 7
    case TG1WDT_SYS_RESET:
        return F("Timer Group1 Watch dog reset digital core"); // 8
    case RTCWDT_SYS_RESET:
        return F("RTC Watch dog Reset digital core"); // 9
    case INTRUSION_RESET:
        return F("Instrusion tested to reset CPU"); // 10
    case TGWDT_CPU_RESET:
        return F("Time Group reset CPU"); // 11
    case SW_CPU_RESET:
        return F("Software reset CPU"); // 12
    case RTCWDT_CPU_RESET:
        return F("RTC Watch dog Reset CPU"); // 13
    case EXT_CPU_RESET:
        return F("or APP CPU, reseted by PRO CPU"); // 14
    case RTCWDT_BROWN_OUT_RESET:
        return F("Reset when the vdd voltage is not stable"); // 15
    case RTCWDT_RTC_RESET:
        return F("RTC Watch dog reset digital core and rtc module"); // 16
    default:
        return F("NO_MEAN"); // 0
    }
}

String ESP_getResetReason(void)
{
    return ESP32GetResetReason(0); // CPU 0
}

uint32_t ESP_getSketchSize(void)
{
    static uint32_t sketchsize = 0;

    if (!sketchsize)
    {
        sketchsize = ESP.getSketchSize(); // This takes almost 2 seconds on an ESP32
    }
    return sketchsize;
}

uint8_t pwm_channel[8] = {99, 99, 99, 99, 99, 99, 99, 99};
uint8_t pin2chan(uint8_t pin)
{
    for (uint8_t cnt = 0; cnt < 8; cnt++)
    {
        if (pwm_channel[cnt] == 99)
        {
            pwm_channel[cnt] = pin;
            uint8_t ch = cnt + PWM_CHANNEL_OFFSET;
            ledcSetup(ch, 1000, 10);
            ledcAttachPin(pin, ch);
            return cnt;
        }
        if ((pwm_channel[cnt] < 99) && (pwm_channel[cnt] == pin))
        {
            return cnt;
        }
    }
    return 0;
}

void analogWrite(uint8_t pin, int val)
{
    uint8_t channel = pin2chan(pin);
    ledcWrite(channel + PWM_CHANNEL_OFFSET, val);
}

int8_t readUserData(size_t src_offset, void *dst, size_t size)
{
    const esp_partition_t *find_partition = esp_partition_find_first((esp_partition_type_t)0x40, (esp_partition_subtype_t)0x0, NULL);
    if (find_partition == NULL)
    {
        Serial.print("No partition found!");
        return -1;
    }

    if (esp_partition_read(find_partition, src_offset, dst, size) != ESP_OK)
    {
        Serial.print("Read partition data error");
        return -2;
    }
    return 0;
}

int8_t writeUserData(size_t dst_offset, const void *src, size_t size)
{
    const esp_partition_t *find_partition = esp_partition_find_first((esp_partition_type_t)0x40, (esp_partition_subtype_t)0x0, NULL);
    if (find_partition == NULL)
    {
        Serial.print("No partition found!");
        return -1;
    }

    if (dst_offset + size > find_partition->size)
    {
        Serial.print("error size");
        return -1;
    }

    uint8_t sector = (dst_offset / SPI_FLASH_SEC_SIZE);
    uint8_t copy[SPI_FLASH_SEC_SIZE];

    if (esp_partition_read(find_partition, sector * SPI_FLASH_SEC_SIZE, copy, SPI_FLASH_SEC_SIZE) != ESP_OK)
    {
        Serial.print("Read partition data error");
        return -2;
    }

    if (esp_partition_erase_range(find_partition, sector * SPI_FLASH_SEC_SIZE, SPI_FLASH_SEC_SIZE) != ESP_OK)
    {
        Serial.print("Erase partition error");
        return -3;
    }

    memcpy(&copy[dst_offset % SPI_FLASH_SEC_SIZE], src, size);

    if (esp_partition_write(find_partition, sector * SPI_FLASH_SEC_SIZE, copy, SPI_FLASH_SEC_SIZE) != ESP_OK)
    {
        Serial.print("Write partition data error");
        return -4;
    }

    return 0;
}

#endif