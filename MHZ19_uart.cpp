/*
  MHZ19_uart.cpp - MH-Z19 CO2 sensor library for ESP-WROOM-02/32(ESP8266/ESP32) or Arduino
  version 0.3
  
  License MIT
*/

#include "MHZ19_uart.h"
#include "Arduino.h"

// public
MHZ19_uart::MHZ19_uart()
{
}
MHZ19_uart::MHZ19_uart(int rx, int tx)
{
	begin(rx, tx);
}

MHZ19_uart::~MHZ19_uart()
{
#ifndef ARDUINO_ARCH_ESP32
	delete _software_serial;
#endif
}

void MHZ19_uart::begin(int rx, int tx)
{
	_rx_pin = rx;
	_tx_pin = tx;
#ifdef ARDUINO_ARCH_ESP32
	Serial1.begin(9600, SERIAL_8N1, _rx_pin, _tx_pin);
#else
	delete _software_serial;
	_software_serial = new SoftwareSerial(_rx_pin, _tx_pin);
	_software_serial->begin(9600);
#endif
}

void MHZ19_uart::setAutoCalibration(boolean autocalib)
{
	writeCommand(autocalib ? autocalib_on : autocalib_off);
}

bool MHZ19_uart::startCO2Read()
{
	if (_read_status != AsyncReadStatus::Idle)
	{
		return false;
	}

	Stream *serial = serialPort();
	if (serial == NULL)
	{
		_read_status = AsyncReadStatus::Failure;
		return true;
	}

	while (serial->available() > 0)
	{
		serial->read();
	}

	resetResponseParser();
	writeCommand(getppm);
	_response_deadline = millis() + RESPONSE_TIMEOUT_MS;
	_read_status = AsyncReadStatus::Pending;
	return true;
}

AsyncReadStatus MHZ19_uart::pollCO2(int &ppm)
{
	if (_read_status == AsyncReadStatus::Idle)
	{
		return AsyncReadStatus::Idle;
	}
	if (_read_status == AsyncReadStatus::Failure)
	{
		_read_status = AsyncReadStatus::Idle;
		return AsyncReadStatus::Failure;
	}

	Stream *serial = serialPort();
	uint8_t bytesRead = 0;
	while (serial != NULL && serial->available() > 0 && bytesRead < MAX_BYTES_PER_POLL)
	{
		int value = serial->read();
		if (value < 0)
		{
			break;
		}
		bytesRead++;
		uint8_t byteValue = (uint8_t) value;

		if (_response_index == 0)
		{
			if (byteValue == 0xff)
			{
				_response[0] = byteValue;
				_response_index = 1;
			}
			continue;
		}

		if (_response_index == 1)
		{
			if (byteValue == 0x86)
			{
				_response[1] = byteValue;
				_response_index = 2;
			}
			else if (byteValue != 0xff)
			{
				_response_index = 0;
			}
			continue;
		}

		_response[_response_index++] = byteValue;
		if (_response_index < RESPONSE_CNT)
		{
			continue;
		}

		if (mhz19_checksum(_response) == _response[RESPONSE_CNT - 1])
		{
			int parsedPpm = _response[2] * 256 + _response[3];
			if (parsedPpm > 0)
			{
				ppm = parsedPpm;
				_read_status = AsyncReadStatus::Idle;
				return AsyncReadStatus::Success;
			}

			_read_status = AsyncReadStatus::Idle;
			return AsyncReadStatus::Failure;
		}

		// A malformed candidate does not end the request. Retain any embedded
		// header (or trailing 0xff) as the beginning of the next candidate.
		uint8_t retainedBytes = 0;
		for (uint8_t i = 1; i < RESPONSE_CNT; i++)
		{
			if (_response[i] != 0xff)
			{
				continue;
			}
			if (i == RESPONSE_CNT - 1)
			{
				_response[0] = 0xff;
				retainedBytes = 1;
				break;
			}
			if (_response[i + 1] == 0x86)
			{
				retainedBytes = RESPONSE_CNT - i;
				for (uint8_t j = 0; j < retainedBytes; j++)
				{
					_response[j] = _response[i + j];
				}
				break;
			}
		}
		_response_index = retainedBytes;
	}

	if (responseDeadlineReached(millis()))
	{
		_read_status = AsyncReadStatus::Idle;
		return AsyncReadStatus::Failure;
	}

	return AsyncReadStatus::Pending;
}

// protected
void MHZ19_uart::writeCommand(uint8_t cmd[])
{
	Stream *serial = serialPort();
	if (serial != NULL)
	{
		serial->write(cmd, REQUEST_CNT);
		serial->write(mhz19_checksum(cmd));
	}
}

// private
Stream *MHZ19_uart::serialPort()
{
#ifdef ARDUINO_ARCH_ESP32
	return &Serial1;
#else
	return _software_serial;
#endif
}

void MHZ19_uart::resetResponseParser()
{
	_response_index = 0;
}

bool MHZ19_uart::responseDeadlineReached(uint32_t now) const
{
	return (int32_t) (now - _response_deadline) >= 0;
}

uint8_t MHZ19_uart::mhz19_checksum(uint8_t com[])
{
	uint8_t sum = 0x00;
	for (int i = 1; i < MHZ19_uart::REQUEST_CNT; i++)
	{
		sum += com[i];
	}
	sum = 0xff - sum + 0x01;
	return sum;
}
