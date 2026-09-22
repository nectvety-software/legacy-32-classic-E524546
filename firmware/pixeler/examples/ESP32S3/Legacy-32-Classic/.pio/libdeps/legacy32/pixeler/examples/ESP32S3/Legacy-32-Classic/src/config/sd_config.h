#pragma once
#include <stdint.h>

#define SD_MOUNTPOINT "/sd"
#define SD_MAX_FILES 10

#define SD_TYPE_MMC

#define SDMMC_FREQ 40000
#define SDMMC_SLOT SDMMC_HOST_SLOT_0
#define SDMMC_PIN_CLK 13
#define SDMMC_PIN_CMD 11
#define SDMMC_PIN_D0 9

// The board exposes DAT3/CS on GPIO10, but 1-bit SDMMC only needs DAT0.
#define SDMMC_PIN_D1 GPIO_NUM_NC
#define SDMMC_PIN_D2 GPIO_NUM_NC
#define SDMMC_PIN_D3 GPIO_NUM_NC
#define SDMMC_MODE_1_BIT
#define SDMMC_POWER_CHANNEL GPIO_NUM_NC

