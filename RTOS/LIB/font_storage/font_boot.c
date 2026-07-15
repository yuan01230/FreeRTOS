#include "font_boot.h"

#include "font_storage.h"
#include "EN25Q128.h"
#include "../UART/app_uart.h"
#include "fatfs.h"
#include "ff.h"
#include <stdio.h>
#include <string.h>

#define FONT_BOOT_SD_PATH "0:/Font/HZK16"
#define FONT_BOOT_SD_DIR  "0:/Font"
#define FONT_BOOT_SD_FILE "HZK16"
#define FONT_BOOT_MAX_DIR_ENTRIES 64U

typedef struct
{
    FontBootStatusCallback cb;
    void *user;
} FontBootProgressContext;

static void FontBoot_Report(FontBootStatusCallback cb, void *user, const char *stage, uint8_t percent)
{
    if (cb != 0)
    {
        cb(stage, percent, user);
    }
}

static void FontBoot_ImportProgress(uint32_t done, uint32_t total, void *user)
{
    FontBootProgressContext *ctx = (FontBootProgressContext *)user;
    uint8_t percent = 0U;

    if (ctx == 0 || ctx->cb == 0)
    {
        return;
    }

    if (total != 0U)
    {
        percent = (uint8_t)((done * 100UL) / total);
    }

    ctx->cb("Importing...", percent, ctx->user);
}

static FRESULT FontBoot_CheckFontFile(void)
{
    DIR dir;
    FILINFO info;
    FRESULT fres;
    uint16_t entry_count = 0U;
    char message[96];

    fres = f_opendir(&dir, FONT_BOOT_SD_DIR);
    if (fres != FR_OK)
    {
        snprintf(message, sizeof(message), "[font] open dir %s failed fr=%u\r\n",
                 FONT_BOOT_SD_DIR, (unsigned int)fres);
        App_UART_Print(message);
        return fres;
    }

    for (;;)
    {
        fres = f_readdir(&dir, &info);
        if (fres != FR_OK)
        {
            snprintf(message, sizeof(message), "[font] read dir failed fr=%u\r\n",
                     (unsigned int)fres);
            App_UART_Print(message);
            (void)f_closedir(&dir);
            return fres;
        }

        if (info.fname[0] == '\0')
        {
            App_UART_Print("[font] HZK16 not found in 0:/Font\r\n");
            (void)f_closedir(&dir);
            return FR_NO_FILE;
        }

        snprintf(message, sizeof(message), "[font] dir entry: %.24s attr=0x%02x size=%lu\r\n",
                 info.fname,
                 (unsigned int)info.fattrib,
                 (unsigned long)info.fsize);
        App_UART_Print(message);

        if ((info.fattrib & AM_DIR) == 0U)
        {
            if ((strcmp(info.fname, FONT_BOOT_SD_FILE) == 0) ||
                (strcmp(info.altname, FONT_BOOT_SD_FILE) == 0))
            {
                (void)f_closedir(&dir);
                return FR_OK;
            }
        }

        entry_count++;
        if (entry_count >= FONT_BOOT_MAX_DIR_ENTRIES)
        {
            App_UART_Print("[font] stop dir scan after 64 entries\r\n");
            (void)f_closedir(&dir);
            return FR_NO_FILE;
        }
    }
}

FontBootResult FontBoot_EnsureReadyEx(uint8_t *sd_mounted_out, FontBootStatusCallback cb, void *user)
{
    FRESULT fres;
    FontStorageResult import_res;
    FontBootProgressContext progress_ctx;

    if (sd_mounted_out != 0)
    {
        *sd_mounted_out = 0U;
    }

    progress_ctx.cb = cb;
    progress_ctx.user = user;

    FontBoot_Report(cb, user, "Init Flash...", 0U);
    EN25QXX_Init();

    FontBoot_Report(cb, user, "Check Font...", 0U);
    FontStorage_Init();
    if (FontStorage_IsReady() != 0U)
    {
        FontBoot_Report(cb, user, "Font Ready", 100U);
        return FONT_BOOT_READY;
    }

    FontBoot_Report(cb, user, "Mount SD...", 0U);
    fres = f_mount(&SDFatFS, SDPath, 1);
    if (fres != FR_OK)
    {
        FontBoot_Report(cb, user, "Mount SD Failed", (uint8_t)fres);
        return FONT_BOOT_MOUNT_FAILED;
    }

    if (sd_mounted_out != 0)
    {
        *sd_mounted_out = 1U;
    }

    FontBoot_Report(cb, user, "Open HZK16...", 0U);
    fres = FontBoot_CheckFontFile();
    if ((fres == FR_NO_FILE) || (fres == FR_NO_PATH))
    {
        FontBoot_Report(cb, user, "HZK16 Missing", 0U);
        return FONT_BOOT_FONT_FILE_MISSING;
    }
    if (fres != FR_OK)
    {
        FontBoot_Report(cb, user, "HZK16 Stat Err", (uint8_t)fres);
        return FONT_BOOT_IMPORT_FAILED;
    }

    import_res = FontStorage_ImportFromSDEx(FONT_BOOT_SD_PATH, FontBoot_ImportProgress, &progress_ctx);
    if (import_res != FONT_STORAGE_OK)
    {
        FontBoot_Report(cb, user, "Import Failed", 0U);
        return FONT_BOOT_IMPORT_FAILED;
    }

    FontBoot_Report(cb, user, "Verify Font...", 100U);
    FontStorage_Init();
    if (FontStorage_IsReady() == 0U)
    {
        FontBoot_Report(cb, user, "Verify Failed", 0U);
        return FONT_BOOT_IMPORT_FAILED;
    }

    FontBoot_Report(cb, user, "Font Ready", 100U);
    return FONT_BOOT_IMPORTED;
}

FontBootResult FontBoot_EnsureReady(uint8_t *sd_mounted_out)
{
    return FontBoot_EnsureReadyEx(sd_mounted_out, 0, 0);
}

const char *FontBoot_ResultToString(FontBootResult result)
{
    switch (result)
    {
        case FONT_BOOT_READY:
            return "font ready in external flash";
        case FONT_BOOT_IMPORTED:
            return "font imported from SD to external flash";
        case FONT_BOOT_NO_SD:
            return "sd card not available";
        case FONT_BOOT_MOUNT_FAILED:
            return "sd mount failed";
        case FONT_BOOT_FONT_FILE_MISSING:
            return "0:/Font/HZK16 missing";
        case FONT_BOOT_IMPORT_FAILED:
        default:
            return "font import failed";
    }
}
