#pragma once
#include <stdint.h>
#include "board_config.h"

#define SD_MOUNTPOINT "/sd"
#define SD_MAX_FILES 10

#define SD_TYPE_MMC

#define SDMMC_FREQ 40000
#define SDMMC_SLOT SDMMC_HOST_SLOT_0
#define SDMMC_PIN_CLK BOARD_SD_CLK
#define SDMMC_PIN_CMD BOARD_SD_CMD
#define SDMMC_PIN_D0 BOARD_SD_DAT0

// Keep the complete physical connector map. In 1-bit mode the driver uses
// CLK, CMD and DAT0; CD/DAT3 stays assigned to GPIO10 but is not driven.
#define SDMMC_PIN_D1 GPIO_NUM_NC
#define SDMMC_PIN_D2 GPIO_NUM_NC
#define SDMMC_PIN_D3 BOARD_SD_DAT3
#define SDMMC_MODE_1_BIT
#define SDMMC_POWER_CHANNEL GPIO_NUM_NC
