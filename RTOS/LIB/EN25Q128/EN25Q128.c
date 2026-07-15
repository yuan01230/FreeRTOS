//
// Created by ���� on 26-1-2.
//

#include "EN25Q128.h"
#include "../UART/app_uart.h"
#include "stdio.h"
#include "stm32f4xx.h"

uint16_t EN25QXX_TYPE = EN25Q128;	//Ĭ����EN25Q128
extern SPI_HandleTypeDef hspi1;
extern UART_HandleTypeDef huart1;
/**
 * @brief  ����SPI1ͨ���ٶ�
 * @note   SPI�ٶ� = fAPB1 / ��Ƶϵ����fAPB1ʱ��һ��Ϊ42MHz
 * @param  SPI_BaudRatePrescaler: SPI������Ԥ��Ƶֵ
 *         ��ѡֵ: SPI_BAUDRATEPRESCALER_2 ~ SPI_BAUDRATEPRESCALER_256
 * @retval ��
 * @optimization ����:
 *         1. ���ӷ���ֵָʾ���óɹ�/ʧ��
 *         2. ���ӻ��Ᵽ��������SPI����������޸��ٶ�
 */
void SPI1_SetSpeed(uint8_t SPI_BaudRatePrescaler)
{
	assert_param(IS_SPI_BAUDRATE_PRESCALER(SPI_BaudRatePrescaler));//�ж���Ч��
	__HAL_SPI_DISABLE(&hspi1); //�ر�SPI
	hspi1.Instance->CR1&=0XFFC7; //λ3-5 ���㣬�������ò�����
	hspi1.Instance->CR1|=SPI_BaudRatePrescaler;//����SPI �ٶ�
	__HAL_SPI_ENABLE(&hspi1); //ʹ��SPI
}

/**
 * @brief  ��ʼ��EN25QXX FlashоƬ
 * @note   EN25Q128���:
 *         - ����: 16MB (128Mbit)
 *         - Sector: 4KB����4096��
 *         - Block: 64KB (16��Sector)����256��
 * @param  ��
 * @retval ��
 * @optimization ����:
 *         1. ���ӷ���ֵ���ID�Ƿ�ƥ��Ԥ���ͺ�
 *         2. ���ӳ�ʼ��ʧ�ܴ�������
 *         3. ��ѡ����SPI�ٶȲ���
 */
void EN25QXX_Init(void)
{
	//Ƭѡ���ߣ�ȡ��ѡ��FlashоƬ
	HAL_GPIO_WritePin(SPI1_CS_GPIO_Port,SPI1_CS_Pin,SET);

	// SPI1_SetSpeed(SPI_BAUDRATEPRESCALER_2);
	EN25QXX_TYPE=EN25QXX_ReadID();	//��ȡFLASH ID.
	{
		char log_text[48];
		snprintf(log_text, sizeof(log_text), "[flash] id=0x%04X sr=0x%02X\r\n", EN25QXX_TYPE, EN25QXX_ReadSR());
		App_UART_Print(log_text);
	}

}

/**
 * @brief  ��ȡEN25QXX��״̬�Ĵ���
 * @note   ״̬�Ĵ���λ����:
 *         BIT7  6   5   4   3   2   1   0
 *         SPR   RV  TB BP2 BP1 BP0 WEL BUSY
 *         - SPR: ״̬�Ĵ�������λ(���WP����ʹ��)
 *         - TB,BP2,BP1,BP0: Flash����д��������
 *         - WEL: дʹ������λ(1=дʹ��)
 *         - BUSY: æ���λ(1=æ, 0=����)
 *         Ĭ��ֵ: 0x00
 * @param  ��
 * @retval ״̬�Ĵ�����ֵ
 */
uint8_t EN25QXX_ReadSR(void)
{
	uint8_t tx_buf[2] = {EN25X_ReadStatusReg, 0xFF};
	uint8_t rx_buf[2];
	
	// Ƭѡ����
	HAL_GPIO_WritePin(SPI1_CS_GPIO_Port, SPI1_CS_Pin, RESET);
	
	// ʹ��TransmitReceiveһ������ɶ�ȡ
	HAL_SPI_TransmitReceive(&hspi1, tx_buf, rx_buf, 2, HAL_MAX_DELAY);
	
	// Ƭѡ����
	HAL_GPIO_WritePin(SPI1_CS_GPIO_Port, SPI1_CS_Pin, SET);
	
	return rx_buf[1];  // ����ʵ�ʶ�ȡ��״̬�Ĵ���ֵ
}

/**
 * @brief  дEN25QXX״̬�Ĵ���
 * @note   ֻ��SPR,TB,BP2,BP1,BP0(bit 7,5,4,3,2)����д
 *         д״̬�Ĵ���ǰ��Ҫ�ȷ���дʹ������
 * @param  sr: Ҫд��״̬�Ĵ�����ֵ
 * @retval ��
 */
void EN25QXX_Write_SR(uint8_t sr)
{
	uint8_t cmd = EN25X_WriteStatusReg;
	
	// ��ʹ��д����
	EN25QXX_Write_Enable();
	
	// Ƭѡ����
	HAL_GPIO_WritePin(SPI1_CS_GPIO_Port, SPI1_CS_Pin, RESET);
	
	// ����д״̬�Ĵ�������
	HAL_SPI_Transmit(&hspi1, &cmd, 1, HAL_MAX_DELAY);
	
	// д��״̬�Ĵ���ֵ
	HAL_SPI_Transmit(&hspi1, &sr, 1, HAL_MAX_DELAY);
	
	// Ƭѡ����
	HAL_GPIO_WritePin(SPI1_CS_GPIO_Port, SPI1_CS_Pin, SET);
	
	// �ȴ�д�����
	EN25QXX_Wait_Busy();
}

/**
 * @brief  EN25QXXдʹ������
 * @note   ��ִ��д����(ҳ��̡�����������оƬ����)ǰ�������
 *         ������Ὣ״̬�Ĵ�����WELλ��1
 * @param  ��
 * @retval ��
 * @optimization ����:
 *         1. ����WELλ���óɹ�����֤(��״̬�Ĵ������)
 *         2. ���ӷ���ֵ��ʾ�Ƿ����óɹ�
 */
void EN25QXX_Write_Enable(void)
{
	uint8_t spi_txbyte = 0;
	spi_txbyte = EN25X_WriteEnable;
	//Ƭѡ����
	HAL_GPIO_WritePin(SPI1_CS_GPIO_Port,SPI1_CS_Pin,RESET); //ʹ������
	HAL_SPI_Transmit(&hspi1,(uint8_t *)(&(spi_txbyte)),1,HAL_MAX_DELAY);//����дʹ������
	//Ƭѡ����
	HAL_GPIO_WritePin(SPI1_CS_GPIO_Port,SPI1_CS_Pin,SET);
}

/**
 * @brief  EN25QXXд��ֹ����
 * @note   ������Ὣ״̬�Ĵ�����WELλ���㣬��ֹд����
 *         һ����д������ɺ�Flash���Զ����WELλ
 * @param  ��
 * @retval ��
 * @optimization ����:
 *         1. �˺�����ʵ��Ӧ���н���ʹ�ã�Flash���Զ����WEL
 *         2. ������WELλ����ɹ�����֤
 */
void EN25QXX_Write_Disable(void)
{
	uint8_t spi_txbyte = 0;
	spi_txbyte = EN25X_WriteDisable;
	//Ƭѡ����
	HAL_GPIO_WritePin(SPI1_CS_GPIO_Port,SPI1_CS_Pin,RESET); //ʹ������
	HAL_SPI_Transmit(&hspi1,(uint8_t *)(&(spi_txbyte)),1,HAL_MAX_DELAY);//����д��ֹ����
	//Ƭѡ����
	HAL_GPIO_WritePin(SPI1_CS_GPIO_Port,SPI1_CS_Pin,SET);
}

/**
 * @brief  ��ȡFlashоƬID
 * @note   ʹ������0x90(������/�豸ID����)��ȡ
 *         ��ͬ�ͺŷ���ֵ:
 *         - 0xEF13: EN25Q80
 *         - 0xEF14: EN25Q16
 *         - 0xEF15: EN25Q32
 *         - 0xEF16: EN25Q64
 *         - 0xEF17: EN25Q128
 * @param  ��
 * @retval FlashоƬID (16λ)
 * @optimization ����:
 *         1. ��ʹ��JEDEC ID����(0x9F)��ȡ����������Ϣ
 *         2. ���Ӵ����������ȡʧ�ܷ���0xFFFF
 *         3. ע����˵��ʵ��ʹ�õ�оƬIDֵ(0x6817)
 */
uint16_t EN25QXX_ReadID(void)
{
	uint16_t Temp = 0;

	uint8_t tx_buf[8] = {0x90, 0x00, 0x00, 0x00, 0x00, 0x00}; // ������0x00ֻ���ṩʱ��
	uint8_t rx_buf[8];

	HAL_GPIO_WritePin(SPI1_CS_GPIO_Port, SPI1_CS_Pin, GPIO_PIN_RESET);

	HAL_SPI_TransmitReceive(&hspi1, tx_buf, rx_buf, 6, HAL_MAX_DELAY);

	HAL_GPIO_WritePin(SPI1_CS_GPIO_Port, SPI1_CS_Pin, GPIO_PIN_SET);

	// HAL_UART_Transmit(&huart1, rx_buf, 8, HAL_MAX_DELAY);
	// printf("Temp:%x\r\n",Temp);
	Temp = rx_buf[4]<<8|rx_buf[5];
	return Temp;
}

/**
 * @brief  ��SPI Flash��ȡ����
 * @note   ���Դ������ַ��ʼ��ȡ��û��ҳ�߽�����
 *         ��ȡǰ����Ҫ��������
 * @param  pBuffer: ���ݴ洢������ָ��
 * @param  ReadAddr: ��ȡ��ʼ��ַ(24λ��ַ��0x000000~0xFFFFFF)
 * @param  NumByteToRead: Ҫ��ȡ���ֽ���(���65535)
 * @retval ��
 */
void EN25QXX_Read(uint8_t* pBuffer,uint32_t ReadAddr,uint16_t NumByteToRead)
{
	uint8_t cmd = EN25X_ReadData;
	uint8_t addr[3];
	
	// ������Ч�Լ��
	if(pBuffer == NULL || NumByteToRead == 0) return;
	
	// ��ַ�ֽڲ�֣����ֽ���ǰ��MSB first��
	addr[0] = (ReadAddr >> 16) & 0xFF;  // A23-A16
	addr[1] = (ReadAddr >> 8) & 0xFF;   // A15-A8
	addr[2] = ReadAddr & 0xFF;          // A7-A0
	
	// Ƭѡ���ͣ���ʼͨ��
	HAL_GPIO_WritePin(SPI1_CS_GPIO_Port, SPI1_CS_Pin, RESET);
	
	// ���Ͷ�ȡ����
	HAL_SPI_Transmit(&hspi1, &cmd, 1, HAL_MAX_DELAY);
	
	// ����24λ��ַ�������η��ͣ�ȷ���ֽ�����ȷ��
	HAL_SPI_Transmit(&hspi1, addr, 3, HAL_MAX_DELAY);
	
	// һ���Խ����������ݣ��Ż����ܣ�
	HAL_SPI_Receive(&hspi1, pBuffer, NumByteToRead, HAL_MAX_DELAY);
	
	// Ƭѡ���ߣ�����ͨ��
	HAL_GPIO_WritePin(SPI1_CS_GPIO_Port, SPI1_CS_Pin, SET);
}

/**
 * @brief  ��Flash��һҳ��д������
 * @note   Flashҳ��СΪ256�ֽڣ����ܿ�ҳд��
 *         д��ǰ����ȷ����ַ��Χ�ڵ�����ȫ��Ϊ0xFF
 * @param  pBuffer: ���ݴ洢������ָ��
 * @param  WriteAddr: д����ʼ��ַ(24λ��ַ)
 * @param  NumByteToWrite: Ҫд����ֽ���(���256)����Ӧ������ҳʣ���ֽ���
 * @retval ��
 */
void EN25QXX_Write_Page(uint8_t* pBuffer,uint32_t WriteAddr,uint16_t NumByteToWrite)
{
	uint8_t cmd = EN25X_PageProgram;
	uint8_t addr[3];
	
	// ������Ч�Լ��
	if(pBuffer == NULL || NumByteToWrite == 0 || NumByteToWrite > 256) return;
	
	// ��ַ�ֽڲ�֣����ֽ���ǰ��MSB first��
	addr[0] = (WriteAddr >> 16) & 0xFF;  // A23-A16
	addr[1] = (WriteAddr >> 8) & 0xFF;   // A15-A8
	addr[2] = WriteAddr & 0xFF;          // A7-A0
	
	// ��ʹ��д����
	EN25QXX_Write_Enable();
	
	// Ƭѡ���ͣ���ʼͨ��
	HAL_GPIO_WritePin(SPI1_CS_GPIO_Port, SPI1_CS_Pin, RESET);
	
	// ����ҳ�������
	HAL_SPI_Transmit(&hspi1, &cmd, 1, HAL_MAX_DELAY);
	
	// ����24λ��ַ
	HAL_SPI_Transmit(&hspi1, addr, 3, HAL_MAX_DELAY);
	
	// һ���Է����������ݣ��Ż����ܣ�
	HAL_SPI_Transmit(&hspi1, pBuffer, NumByteToWrite, HAL_MAX_DELAY);
	
	// Ƭѡ���ߣ�����ͨ��
	HAL_GPIO_WritePin(SPI1_CS_GPIO_Port, SPI1_CS_Pin, SET);
	
	// �ȴ�д�����
	EN25QXX_Wait_Busy();
}

/**
 * @brief  ��Flash�����ַд������(��У��)
 * @note   ����ȷ����д�ĵ�ַ��Χ�ڵ�����ȫ��Ϊ0xFF������д��ʧ��
 *         �����Զ���ҳ���ܣ��ɿ�ҳд��
 * @param  pBuffer: ���ݴ洢������ָ��
 * @param  WriteAddr: д����ʼ��ַ(24λ��ַ)
 * @param  NumByteToWrite: Ҫд����ֽ���(���65535)
 * @retval ��
 */
void EN25QXX_Write_NoCheck(uint8_t* pBuffer,uint32_t WriteAddr,uint16_t NumByteToWrite)
{
	uint16_t pageremain;
	
	// ������Ч�Լ��
	if(pBuffer == NULL || NumByteToWrite == 0) return;
	
	// ���㵱ǰҳʣ��ռ�
	pageremain = 256 - (WriteAddr % 256);
	if(NumByteToWrite <= pageremain) {
		pageremain = NumByteToWrite;
	}
	
	while(1)
	{
		// д�뵱ǰҳ
		EN25QXX_Write_Page(pBuffer, WriteAddr, pageremain);
		
		if(NumByteToWrite == pageremain) {
			break;  // д�����
		}
		else {
			// ����������Ҫд��
			pBuffer += pageremain;
			WriteAddr += pageremain;
			NumByteToWrite -= pageremain;
			
			// ������һҳд��������
			if(NumByteToWrite > 256) {
				pageremain = 256;
			} else {
				pageremain = NumByteToWrite;
			}
		}
	}
}

/**
 * @brief  ��Flash�����ַд������(����������)
 * @note   �ú������Զ����Ŀ�������Ƿ���Ҫ����
 *         �����Ҫ���������ȶ�ȡ������������������д��������
 *         �����Զ�������д�빦��
 * @param  pBuffer: ���ݴ洢������ָ��
 * @param  WriteAddr: д����ʼ��ַ(24λ��ַ)
 * @param  NumByteToWrite: Ҫд����ֽ���(���65535)
 * @retval ��
 */
uint8_t EN25QXX_BUFFER[4096];
void EN25QXX_Write(uint8_t* pBuffer,uint32_t WriteAddr,uint16_t NumByteToWrite)
{
	uint32_t secpos;
	uint16_t secoff;
	uint16_t secremain;
 	uint16_t i;
	uint8_t * EN25QXX_BUF;
	
	// ������Ч�Լ��
	if(pBuffer == NULL || NumByteToWrite == 0) return;
	
   	EN25QXX_BUF = EN25QXX_BUFFER;
 	secpos = WriteAddr / 4096;         // �������
	secoff = WriteAddr % 4096;         // �������ڵ�ƫ��
	secremain = 4096 - secoff;         // ����ʣ��ռ�
	
 	if(NumByteToWrite <= secremain) {
		secremain = NumByteToWrite;
	}
	
	while(1)
	{
		// ��ȡ��������������
		EN25QXX_Read(EN25QXX_BUF, secpos * 4096, 4096);
		
		// ����Ƿ���Ҫ����
		for(i = 0; i < secremain; i++) {
			if(EN25QXX_BUF[secoff + i] != 0xFF) {
				break;  // ���ַ�0xFF����Ҫ����
			}
		}
		
		if(i < secremain) {
			// ��Ҫ����
			EN25QXX_Erase_Sector(secpos);
			
			// ���������ݵ�������
			for(i = 0; i < secremain; i++) {
				EN25QXX_BUF[i + secoff] = pBuffer[i];
			}
			
			// д����������
			EN25QXX_Write_NoCheck(EN25QXX_BUF, secpos * 4096, 4096);
		} else {
			// ����Ҫ������ֱ��д��
			EN25QXX_Write_NoCheck(pBuffer, WriteAddr, secremain);
		}
		
		if(NumByteToWrite == secremain) {
			break;  // д�����
		}
		else {
			// ����������Ҫд��
			secpos++;                      // ��һ������
			secoff = 0;                    // ��������ͷ��ʼд
		   	pBuffer += secremain;
			WriteAddr += secremain;
		   	NumByteToWrite -= secremain;
			
			if(NumByteToWrite > 4096) {
				secremain = 4096;
			} else {
				secremain = NumByteToWrite;
			}
		}
	}
}

/**
 * @brief  ��������FlashоƬ
 * @note   ����ʱ��ǳ���������оƬ�ͺŲ�ͬ��������Ҫ������
 *         �������������ݱ�Ϊ0xFF
 * @param  ��
 * @retval ��
 */
void EN25QXX_Erase_Chip(void)
{
	uint8_t cmd = EN25X_ChipErase;
	
	// ʹ��д����
	EN25QXX_Write_Enable();
	
	// Ƭѡ����
	HAL_GPIO_WritePin(SPI1_CS_GPIO_Port, SPI1_CS_Pin, RESET);
	
	// ����оƬ��������
	HAL_SPI_Transmit(&hspi1, &cmd, 1, HAL_MAX_DELAY);
	
	// Ƭѡ����
	HAL_GPIO_WritePin(SPI1_CS_GPIO_Port, SPI1_CS_Pin, SET);
	
	// �ȴ�оƬ��������
	EN25QXX_Wait_Busy();
}

/**
 * @brief  ����Flash��һ������(4KB)
 * @note   ������СΪ4096�ֽ�(4KB)
 *         ����һ��������ʱ��Լ150ms
 * @param  Dst_Addr: �������(0~4095)�����Զ���4096
 * @retval ��
 */
void EN25QXX_Erase_Sector(uint32_t Dst_Addr)
{
	uint8_t cmd = EN25X_SectorErase;
	uint8_t addr[3];
	uint32_t sector_addr;
	char log_text[96];

	sector_addr = Dst_Addr * 4096;
	addr[0] = (sector_addr >> 16) & 0xFF;
	addr[1] = (sector_addr >> 8) & 0xFF;
	addr[2] = sector_addr & 0xFF;

	snprintf(log_text, sizeof(log_text), "[flash] erase sector=%lu addr=0x%06lX sr0=0x%02X\r\n",
			 (unsigned long)Dst_Addr, (unsigned long)sector_addr, EN25QXX_ReadSR());
	App_UART_Print(log_text);

	EN25QXX_Write_Enable();
	snprintf(log_text, sizeof(log_text), "[flash] after WREN sr=0x%02X\r\n", EN25QXX_ReadSR());
	App_UART_Print(log_text);

	HAL_GPIO_WritePin(SPI1_CS_GPIO_Port, SPI1_CS_Pin, RESET);
	HAL_SPI_Transmit(&hspi1, &cmd, 1, HAL_MAX_DELAY);
	HAL_SPI_Transmit(&hspi1, addr, 3, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(SPI1_CS_GPIO_Port, SPI1_CS_Pin, SET);

	EN25QXX_Wait_Busy();
}

/**
 * @brief  �ȴ�Flash����
 * @note   ͨ����ѯ״̬�Ĵ�����BUSYλ(bit 0)���ж�
 *         BUSY=1��ʾFlashæ��BUSY=0��ʾ����
 *         �����ӳ�ʱ��������ֹӲ���쳣������ѭ��
 * @param  ��
 * @retval ��
 */
void EN25QXX_Wait_Busy(void)
{
	uint32_t timeout = 3000;
	uint8_t sr;

	while(((sr = EN25QXX_ReadSR()) & 0x01) == 0x01) {
		if(--timeout == 0) {
			char log_text[48];
			snprintf(log_text, sizeof(log_text), "[flash] wait busy timeout sr=0x%02X\r\n", sr);
			App_UART_Print(log_text);
			return;
		}
		HAL_Delay(1);
	}
}

/**
 * @brief  ʹFlashоƬ�������ģʽ
 * @note   ����ģʽ�¹��ļ��ͣ����޷�����Flash
 *         �������ģʽ����Ҫ�ȴ�tPD(Լ3us)
 * @param  ��
 * @retval ��
 */
void EN25QXX_PowerDown(void)
{
	uint8_t cmd = EN25X_PowerDown;
	
	// Ƭѡ����
	HAL_GPIO_WritePin(SPI1_CS_GPIO_Port, SPI1_CS_Pin, RESET);
	
	// ���͵�������
	HAL_SPI_Transmit(&hspi1, &cmd, 1, HAL_MAX_DELAY);
	
	// Ƭѡ����
	HAL_GPIO_WritePin(SPI1_CS_GPIO_Port, SPI1_CS_Pin, SET);
	
	// �ȴ�tPD
	HAL_Delay(3);
}

/**
 * @brief  ����FlashоƬ(�˳�����ģʽ)
 * @note   �ӵ���ģʽ���Ѻ���Ҫ�ȴ�tRES1(Լ3us)
 *         ���Ѻ���ܽ���������д����
 * @param  ��
 * @retval ��
 */
void EN25QXX_WAKEUP(void)
{
	uint8_t cmd = EN25X_ReleasePowerDown;
	
	// Ƭѡ����
	HAL_GPIO_WritePin(SPI1_CS_GPIO_Port, SPI1_CS_Pin, RESET);
	
	// ���ͻ������� 0xAB
	HAL_SPI_Transmit(&hspi1, &cmd, 1, HAL_MAX_DELAY);
	
	// Ƭѡ����
	HAL_GPIO_WritePin(SPI1_CS_GPIO_Port, SPI1_CS_Pin, SET);
	
	// �ȴ�tRES1
	HAL_Delay(3);
}



