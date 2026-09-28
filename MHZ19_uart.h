/*
  MHZ19_uart.h - MH-Z19 CO2 sensor library for ESP-WROOM-02/32(ESP8266/ESP32) or Arduino
  version 0.3
  
  License MIT
*/

#ifndef MHZ19_uart_h_
#define MHZ19_uart_h_

#include "Arduino.h"
#ifdef ARDUINO_ARCH_ESP32
#include "HardwareSerial.h"
#else
#include "SoftwareSerial.h"
#endif

enum class AsyncReadStatus : uint8_t
{
	Idle,
	Pending,
	Success,
	Failure
};

class MHZ19_uart
{
public:
	MHZ19_uart();
	MHZ19_uart(int rx, int tx);
	virtual ~MHZ19_uart();

	void begin(int rx = -1, int tx = -1);
	void setAutoCalibration(boolean autocalib);
	bool startCO2Read();
	AsyncReadStatus pollCO2(int &ppm);

protected:
	void writeCommand(uint8_t com[]);

private:
	uint8_t mhz19_checksum(uint8_t com[]);
	Stream *serialPort();
	void resetResponseParser();
	bool responseDeadlineReached(uint32_t now) const;

	static const int REQUEST_CNT = 8;
	static const int RESPONSE_CNT = 9;
	static const uint16_t RESPONSE_TIMEOUT_MS = 1000;
	static const uint8_t MAX_BYTES_PER_POLL = 32;

	AsyncReadStatus _read_status = AsyncReadStatus::Idle;
	uint8_t _response[RESPONSE_CNT] = {};
	uint8_t _response_index = 0;
	uint32_t _response_deadline = 0;

	// serial command
	uint8_t getppm[REQUEST_CNT] = {0xff, 0x01, 0x86, 0x00, 0x00, 0x00, 0x00, 0x00};
	// uint8_t zerocalib[REQUEST_CNT] = {0xff, 0x01, 0x87, 0x00, 0x00, 0x00, 0x00, 0x00};
	// uint8_t spancalib[REQUEST_CNT] = {0xff, 0x01, 0x88, 0x00, 0x00, 0x00, 0x00, 0x00};
	uint8_t autocalib_on[REQUEST_CNT] = {0xff, 0x01, 0x79, 0xA0, 0x00, 0x00, 0x00, 0x00};
	uint8_t autocalib_off[REQUEST_CNT] = {0xff, 0x01, 0x79, 0x00, 0x00, 0x00, 0x00, 0x00};
	int _rx_pin = -1;
	int _tx_pin = -1;
#ifndef ARDUINO_ARCH_ESP32
	SoftwareSerial *_software_serial = NULL;
#endif
};

#endif
