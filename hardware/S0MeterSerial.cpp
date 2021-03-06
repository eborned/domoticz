#include "stdafx.h"
#include "S0MeterSerial.h"
#include "../main/Logger.h"
#include "../main/Helper.h"
#include "../main/RFXtrx.h"
#include "P1MeterBase.h"
#include "hardwaretypes.h"
#include <string>
#include <algorithm>
#include <iostream>
#include <boost/bind.hpp>

#include <ctime>

#ifdef _DEBUG
	//#define DEBUG_S0
	#define TOT_DEBUG_LINES 6
	const char *szDebugDataP2[TOT_DEBUG_LINES] = {
		"/27243:S0 Pulse Counter V0.1\n",
		"ID:27243:I:10:M1:264:264:M2:0:0\n",
		"ID:27243:I:10:M1:983:1247:M2:518:518\n",
		"ID:27243:I:10:M1:1121:2368:M2:0:518\n",
		"ID:27243:I:10:M1:0:2368:M2:1126:1644\n",
		"ID:27243:I:10:M1:921:3289:M2:0:1644\n",
	};
#endif

S0MeterSerial::S0MeterSerial(const int ID, const std::string& devname, const unsigned int baud_rate, const int M1Type, const int M1PPH, const int M2Type, const int M2PPH)
{
	m_HwdID=ID;
	m_szSerialPort=devname;
	m_iBaudRate=baud_rate;

	//Quick hack, meters 3 till 5 uses same parameter as 1 for now, till we can configure it
	m_meters[0].m_type=M1Type;
	m_meters[1].m_type=M2Type;
	m_meters[2].m_type=M1Type;
	m_meters[3].m_type=M1Type;
	m_meters[4].m_type=M1Type;
	m_meters[0].m_pulse_per_unit=1000.0;
	m_meters[1].m_pulse_per_unit=1000.0;
	m_meters[2].m_pulse_per_unit=1000.0;
	m_meters[3].m_pulse_per_unit=1000.0;
	m_meters[4].m_pulse_per_unit=1000.0;

	if (M1PPH!=0)
	{
		m_meters[0].m_pulse_per_unit=float(M1PPH);
		m_meters[1].m_pulse_per_unit=float(M1PPH);
		m_meters[2].m_pulse_per_unit=float(M1PPH);
		m_meters[3].m_pulse_per_unit=float(M1PPH);
		m_meters[4].m_pulse_per_unit=float(M1PPH);
	}
	if (M2PPH!=0)
		m_meters[1].m_pulse_per_unit=float(M2PPH);
}

S0MeterSerial::~S0MeterSerial()
{
	clearReadCallback();
}

bool S0MeterSerial::StartHardware()
{
	StartHeartbeatThread();
	//Try to open the Serial Port
	try
	{
		_log.Log(LOG_STATUS,"S0 Meter: Using serial port: %s", m_szSerialPort.c_str());
#ifndef WIN32
		openOnlyBaud(
			m_szSerialPort,
			m_iBaudRate,
			boost::asio::serial_port_base::parity(boost::asio::serial_port_base::parity::even),
			boost::asio::serial_port_base::character_size(7)
			);
#else
		open(
			m_szSerialPort,
			m_iBaudRate,
			boost::asio::serial_port_base::parity(boost::asio::serial_port_base::parity::even),
			boost::asio::serial_port_base::character_size(7)
			);
#endif
	}
	catch (boost::exception & e)
	{
		_log.Log(LOG_ERROR,"S0 Meter: Error opening serial port!");
#ifdef _DEBUG
		_log.Log(LOG_ERROR,"-----------------\n%s\n-----------------",boost::diagnostic_information(e).c_str());
#endif
		return false;
	}
	catch ( ... )
	{
		_log.Log(LOG_ERROR,"S0 Meter: Error opening serial port!!!");
		return false;
	}
	m_bIsStarted=true;
	m_bufferpos=0;
	ReloadLastTotals();
	setReadCallback(boost::bind(&S0MeterSerial::readCallback, this, _1, _2));
	sOnConnected(this);

#ifdef DEBUG_S0
	int ii = 0;
	for (ii = 0; ii < TOT_DEBUG_LINES; ii++)
	{
		std::string dline = szDebugDataP2[ii];
		ParseData((const unsigned char*)dline.c_str(), dline.size());
	}
#endif

	return true;
}

bool S0MeterSerial::StopHardware()
{
	m_bIsStarted=false;
	if (isOpen())
	{
		try {
			clearReadCallback();
			close();
			doClose();
			setErrorStatus(true);
		} catch(...)
		{
			//Don't throw from a Stop command
		}
	}
	StopHeartbeatThread();
	return true;
}


void S0MeterSerial::readCallback(const char *data, size_t len)
{
	boost::lock_guard<boost::mutex> l(readQueueMutex);
	if (!m_bIsStarted)
		return;

	if (!m_bEnableReceive)
		return; //receiving not enabled

	ParseData((const unsigned char*)data, static_cast<int>(len));
}

void S0MeterSerial::WriteToHardware(const char *pdata, const unsigned char length)
{
}

