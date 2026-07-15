#include "font_storage.h"

#include "EN25Q128.h"
#include "../UART/app_uart.h"
#include "fatfs.h"

#include <stdio.h>
#include <string.h>

/*
 * ˵����
 * - ���ļ������ֿ�����䵽�ⲿ Flash����������ʱ��ƫ�ƶ�����ģ����
 * - �ϲ�ֻ���������£�
 *   1. �ֿ��Ƿ��Ѿ�������
 *   2. �ܷ�� SD ���µ����ֿ⣻
 *   3. ������ģƫ�ƺ��ܷ������Ӧ 16x16 �������ݡ�
 */

/* �ֿ�ͷħ����'FZ16'�� */
#define FONT_STORAGE_MAGIC           0x465A3136UL
/* �ֿ�ͷ�汾�ţ����ڼ������ж� */
#define FONT_STORAGE_VERSION         0x00010000UL
/* �ֿ��� EN25Q128 �е���ʼ��ַ */
#define FONT_STORAGE_FLASH_BASE      0x00F00000UL
/* ��ǰ����������ֿ��С��512KB�� */
#define FONT_STORAGE_MAX_SIZE        (512UL * 1024UL)
/* 16x16 ��ģ�̶�Ϊ 32 �ֽ� */
#define FONT_STORAGE_GLYPH_SIZE      32U
/* SD -> Flash ����ʱ�ķֿ��С */
#define FONT_STORAGE_COPY_CHUNK      256U

/* Flash �е��ֿ�ͷ���� */
typedef struct
{
    uint32_t magic;
    uint32_t version;
    uint32_t font_size;
    uint32_t crc32;
    uint32_t reserved0;
    uint32_t reserved1;
} FontStorageHeader;

/* ����̬���棺�Ƿ���� + �ֿ�ͷ��Ϣ */
static uint8_t g_font_ready = 0;
static FontStorageHeader g_header;

/* ���� CRC32 ���㣨����ʽ 0xEDB88320�� */
static uint32_t FontStorage_Crc32Update(uint32_t crc, const uint8_t *data, uint32_t len)
{
    uint32_t i;
    uint32_t j;

    for (i = 0; i < len; i++)
    {
        crc ^= data[i];
        for (j = 0; j < 8; j++)
        {
            if (crc & 1U)
            {
                crc = (crc >> 1) ^ 0xEDB88320UL;
            }
            else
            {
                crc >>= 1;
            }
        }
    }

    return crc;
}

/* У���ֿ�ͷ�Ƿ�Ϸ���magic/version/size�� */
static uint8_t FontStorage_ValidateHeader(const FontStorageHeader *header)
{
    if (header->magic != FONT_STORAGE_MAGIC)
    {
        return 0;
    }

    if (header->version != FONT_STORAGE_VERSION)
    {
        return 0;
    }

    if (header->font_size == 0 || header->font_size > FONT_STORAGE_MAX_SIZE)
    {
        return 0;
    }

    return 1;
}

/**
 * @brief ����ʱ��ʼ���ֿ�״̬
 * @details
 * ϵͳ�ϵ�󲻻������ص��ֿ⣬������ȥ�ⲿ Flash ��ȡ�ֿ�ͷ��
 * ���ͷ��Ϣ�Ϸ�������Ϊ��ǰ�������Ѿ��п����ֿ⣬����ֱ�ӽ�������̬��
 */
void FontStorage_Init(void)
{
    EN25QXX_Read((uint8_t *)&g_header, FONT_STORAGE_FLASH_BASE, (uint16_t)sizeof(g_header));
    g_font_ready = FontStorage_ValidateHeader(&g_header);
}

uint8_t FontStorage_IsReady(void)
{
    return g_font_ready;
}

uint32_t FontStorage_GetFontSize(void)
{
    if (!g_font_ready)
    {
        return 0;
    }

    return g_header.font_size;
}

/**
 * @brief �� SD �����ֿ⵽�ⲿ Flash����֧�ֽ��Ȼص�
 * @param path SD �����ֿ��ļ�·��
 * @param cb ���Ȼص�
 * @param user �ص�������
 * @return FontStorageResult ������
 * @details
 * �����������£�
 * 1. �� SD �ֿ��ļ���
 * 2. У���ļ���С�Ƿ�Ϸ���
 * 3. ������Ҫ������ Flash ����������
 * 4. �ֿ��ȡ SD �ļ���д�� Flash��
 * 5. ���������ֿ��ļ��� CRC32��
 * 6. ���д���ֿ�ͷ����Ǵ˴ε�����ɡ�
 */
FontStorageResult FontStorage_ImportFromSDEx(const char *path, FontStorageProgressCallback cb, void *user)
{
    FIL file;
    FRESULT fres;
    UINT read_len;
    uint8_t buf[FONT_STORAGE_COPY_CHUNK];
    uint32_t total_size;
    uint32_t write_addr;
    uint32_t remain;
    uint32_t offset = 0;
    uint32_t crc = 0xFFFFFFFFUL;
    uint32_t i;
    uint32_t sector_start;
    uint32_t sector_count;
    uint32_t copy_units;
    uint32_t total_units;
    uint32_t done_units = 0;
    FontStorageHeader header;
    char log_text[96];

    if (path == 0)
    {
        return FONT_STORAGE_ERR_PARAM;
    }

    /* �� SD �ֿ��ļ� */
    App_UART_Print("[font] import f_open begin\r\n");
    fres = f_open(&file, path, FA_READ);
    if (fres != FR_OK)
    {
        snprintf(log_text, sizeof(log_text), "[font] import f_open failed fr=%u\r\n", (unsigned int)fres);
        App_UART_Print(log_text);
        return FONT_STORAGE_ERR_SD_OPEN;
    }
    App_UART_Print("[font] import f_open ok\r\n");

    /* �ļ���СУ�飨0 < size <= FONT_STORAGE_MAX_SIZE�� */
    total_size = f_size(&file);
    snprintf(log_text, sizeof(log_text), "[font] import size=%lu\r\n", (unsigned long)total_size);
    App_UART_Print(log_text);
    if (total_size == 0 || total_size > FONT_STORAGE_MAX_SIZE)
    {
        (void)f_close(&file);
        return FONT_STORAGE_ERR_FORMAT;
    }

    /* ������Ҫ�����������������ֿ�ͷ + �����ֿ⣩ */
    sector_start = FONT_STORAGE_FLASH_BASE / 4096UL;
    sector_count = (uint32_t)((sizeof(FontStorageHeader) + total_size + 4095UL) / 4096UL);
    copy_units = (uint32_t)((total_size + FONT_STORAGE_COPY_CHUNK - 1UL) / FONT_STORAGE_COPY_CHUNK);
    total_units = sector_count + copy_units;

    snprintf(log_text, sizeof(log_text), "[font] erase sectors start=%lu count=%lu\r\n",
             (unsigned long)sector_start, (unsigned long)sector_count);
    App_UART_Print(log_text);

    for (i = 0; i < sector_count; i++)
    {
        if ((i < 4U) || ((i % 16U) == 0U))
        {
            snprintf(log_text, sizeof(log_text), "[font] erase sector %lu/%lu\r\n",
                     (unsigned long)(i + 1U), (unsigned long)sector_count);
            App_UART_Print(log_text);
        }
        EN25QXX_Erase_Sector(sector_start + i);
        done_units++;
        if (cb != 0)
        {
            cb(done_units, total_units, user);
        }
    }

    App_UART_Print("[font] erase done, copy begin\r\n");

    write_addr = FONT_STORAGE_FLASH_BASE + sizeof(FontStorageHeader);
    remain = total_size;

    /* �ֿ��ȡ SD ��д�� Flash��ͬʱ���� CRC */
    while (remain > 0)
    {
        UINT chunk = (remain > FONT_STORAGE_COPY_CHUNK) ? FONT_STORAGE_COPY_CHUNK : (UINT)remain;
        read_len = 0;

        fres = f_read(&file, buf, chunk, &read_len);
        if (fres != FR_OK)
        {
            (void)f_close(&file);
            return FONT_STORAGE_ERR_SD_READ;
        }

        if (read_len == 0)
        {
            (void)f_close(&file);
            return FONT_STORAGE_ERR_SD_READ;
        }

        EN25QXX_Write(buf, write_addr + offset, (uint16_t)read_len);

        crc = FontStorage_Crc32Update(crc, buf, read_len);

        offset += read_len;
        remain -= read_len;
        done_units++;
        if (cb != 0)
        {
            cb(done_units, total_units, user);
        }
    }

    (void)f_close(&file);

    /* д���ֿ�ͷ����Ǳ��ε����� */
    header.magic = FONT_STORAGE_MAGIC;
    header.version = FONT_STORAGE_VERSION;
    header.font_size = total_size;
    header.crc32 = crc ^ 0xFFFFFFFFUL;
    header.reserved0 = 0;
    header.reserved1 = 0;

    EN25QXX_Write((uint8_t *)&header, FONT_STORAGE_FLASH_BASE, (uint16_t)sizeof(header));

    g_header = header;
    g_font_ready = 1;
    return FONT_STORAGE_OK;
}

FontStorageResult FontStorage_ImportFromSD(const char *path)
{
    return FontStorage_ImportFromSDEx(path, 0, 0);
}

/**
 * @brief ����ģƫ�ƶ�ȡ 16x16 ����
 * @param glyph_offset ��ģƫ��
 * @param glyph_buf32 ��� 32 �ֽڵ��󻺳���
 * @return FontStorageResult ��ȡ���
 * @details
 * �ýӿ�������ʱ��õ��ֿ��ȡ��ڡ�
 * �ϲ�����ɱ�������󣬻�� GB2312 ���������ģƫ�ƴ�������
 * Ȼ���ٰѶ����� 32 �ֽڵ����͸� LCD ���ƺ�����
 */
FontStorageResult FontStorage_ReadGlyph16(uint32_t glyph_offset, uint8_t *glyph_buf32)
{
    uint32_t addr;

    if (glyph_buf32 == 0)
    {
        return FONT_STORAGE_ERR_PARAM;
    }

    /* �ֿ�δ����ʱ����ֹ��ȡ */
    if (!g_font_ready)
    {
        return FONT_STORAGE_ERR_NOT_READY;
    }

    /* ��Խ�磺��ȡ���� [offset, offset+32) ���������ֿⷶΧ�� */
    if (glyph_offset + FONT_STORAGE_GLYPH_SIZE > g_header.font_size)
    {
        return FONT_STORAGE_ERR_OUT_OF_RANGE;
    }

    addr = FONT_STORAGE_FLASH_BASE + sizeof(FontStorageHeader) + glyph_offset;
    EN25QXX_Read(glyph_buf32, addr, FONT_STORAGE_GLYPH_SIZE);

    return FONT_STORAGE_OK;
}
