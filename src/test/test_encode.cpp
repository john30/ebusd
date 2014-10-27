/*
 * Copyright (C) Roland Jax 2014 <roland.jax@liwest.at>
 *
 * This file is part of libebus.
 *
 * libebus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * libebus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with libebus. If not, see http://www.gnu.org/licenses/.
 */

#include "encode.h"
#include <iostream>
#include <iomanip>


using namespace libebus;


int main()
{
	Encode* help_enc = NULL;

	std::cout << std::endl;

	// HEX
	{
		const char* hex[] = {"53 70 65 69 63 68 65 72 20 20"};
		for (size_t i = 0; i < sizeof(hex)/sizeof(hex[0]); i++) {
			help_enc = new EncodeHEX(hex[i]);
			std::cout << "EncodeHEX: " << std::setw(20) << hex[i] << " = " << help_enc->encode() << std::endl;

			delete help_enc;
		}

		std::cout << std::endl;
	}

	// UCH
	{
		const char* uch[] = {"0", "1", "127", "128", "254", "255", "161"};
		for (size_t i = 0; i < sizeof(uch)/sizeof(uch[0]); i++) {
			help_enc = new EncodeUCH(uch[i], "1.0");
			std::cout << "EncodeUCH: " << std::setw(20) << uch[i] << " = " << help_enc->encode() << std::endl;

			delete help_enc;
		}

		std::cout << std::endl;
	}

	// SCH
	{
		const char* sch[] = {"0", "1", "127", "-128", "-2", "-1", "-95"};
		for (size_t i = 0; i < sizeof(sch)/sizeof(sch[0]); i++) {
			help_enc = new EncodeSCH(sch[i], "1.0");
			std::cout << "EncodeSCH: " << std::setw(20) << sch[i] << " = " << help_enc->encode() << std::endl;

			delete help_enc;
		}

		std::cout << std::endl;
	}

	// UIN
	{
		const char* uin[] = {"0", "1", "32767", "32768", "65534", "65535", "41394"};
		for (size_t i = 0; i < sizeof(uin)/sizeof(uin[0]); i++) {
			help_enc = new EncodeUIN(uin[i], "1.0");
			std::cout << "EncodeUIN: " << std::setw(20) << uin[i] << " = " << help_enc->encode() << std::endl;

			delete help_enc;
		}

		std::cout << std::endl;
	}

	// SIN
	{
		const char* sin[] = {"0", "1", "32767", "-32768", "-2", "-1", "-24142"};
		for (size_t i = 0; i < sizeof(sin)/sizeof(sin[0]); i++) {
			help_enc = new EncodeSIN(sin[i], "1.0");
			std::cout << "EncodeSIN: " << std::setw(20) << sin[i] << " = " << help_enc->encode() << std::endl;

			delete help_enc;
		}

		std::cout << std::endl;
	}

	// ULG
	{
		const char* ulg[] = {"0", "1", "2147483647", "2147483648", "4294967294", "4294967295", "2712847316"};
		for (size_t i = 0; i < sizeof(ulg)/sizeof(ulg[0]); i++) {
			help_enc = new EncodeULG(ulg[i], "1.0");
			std::cout << "EncodeULG: " << std::setw(20) << ulg[i] << " = " << help_enc->encode() << std::endl;

			delete help_enc;
		}

		std::cout << std::endl;
	}

	// SLG
	{
		const char* slg[] = {"0", "1", "2147483647", "-2147483648", "-2", "-1", "-1582119980"};
		for (size_t i = 0; i < sizeof(slg)/sizeof(slg[0]); i++) {
			help_enc = new EncodeSLG(slg[i], "1.0");
			std::cout << "EncodeSLG: " << std::setw(20) << slg[i] << " = " << help_enc->encode() << std::endl;

			delete help_enc;
		}

		std::cout << std::endl;
	}

	// FLT
	{
		const char* flt[] = {"0.000", "2.075", "9.522", "9.865", "2.129"};
		for (size_t i = 0; i < sizeof(flt)/sizeof(flt[0]); i++) {
			help_enc = new EncodeFLT(flt[i], "1.0");
			std::cout << "EncodeFLT: " << std::setw(20) << flt[i] << " = " << help_enc->encode() << std::endl;

			delete help_enc;
		}

		std::cout << std::endl;
	}

	// STR
	{
		const char* str[] = {"Speicher  ", "VD600" };
		for (size_t i = 0; i < sizeof(str)/sizeof(str[0]); i++) {
			help_enc = new EncodeSTR(str[i]);
			std::cout << "EncodeSTR: " << std::setw(20) << str[i] << " = " << help_enc->encode() << std::endl;

			delete help_enc;
		}

		std::cout << std::endl;
	}

	// BCD
	{
		const char* bcd[] = {"0", "1", "2", "3", "12", "99"};
		for (size_t i = 0; i < sizeof(bcd)/sizeof(bcd[0]); i++) {
			help_enc = new EncodeBCD(bcd[i], "1.0");
			std::cout << "EncodeBCD: " << std::setw(20) << bcd[i] << " = " << help_enc->encode() << std::endl;

			delete help_enc;
		}

		std::cout << std::endl;
	}

	// D1B
	{
		const char* d1b[] = {"00", "01", "127", "-127", "-128"};
		for (size_t i = 0; i < sizeof(d1b)/sizeof(d1b[0]); i++) {
			help_enc = new EncodeD1B(d1b[i], "1.0");
			std::cout << "EncodeD1B: " << std::setw(20) << d1b[i] << " = " << help_enc->encode() << std::endl;

			delete help_enc;
		}

		std::cout << std::endl;
	}

	// D1C
	{
		const char* d1c[] = {"0", "50", "100"};
		for (size_t i = 0; i < sizeof(d1c)/sizeof(d1c[0]); i++) {
			help_enc = new EncodeD1C(d1c[i], "1.0");
			std::cout << "EncodeD1C: " << std::setw(20) << d1c[i] << " = " << help_enc->encode() << std::endl;

			delete help_enc;
		}

		std::cout << std::endl;
	}

	// D2B
	{
		const char* d2b[] = {"0", "0.00390625", "-0.00390625", "-1", "-128", "-127.99609375", "127.99609375"};
		for (size_t i = 0; i < sizeof(d2b)/sizeof(d2b[0]); i++) {
			help_enc = new EncodeD2B(d2b[i], "1.0");
			std::cout << "EncodeD2B: " << std::setw(20) << d2b[i] << " = " << help_enc->encode() << std::endl;

			delete help_enc;
		}

		std::cout << std::endl;
	}

	// D2C
	{
		const char* d2c[] = {"0", "0.0625", "-0.0625", "-1", "-2048", "-2047.9375", "2047.9375"};
		for (size_t i = 0; i < sizeof(d2c)/sizeof(d2c[0]); i++) {
			help_enc = new EncodeD2C(d2c[i], "1.0");
			std::cout << "EncodeD2C: " << std::setw(20) << d2c[i] << " = " << help_enc->encode() << std::endl;

			delete help_enc;
		}

		std::cout << std::endl;
	}

	// BDA
	{
		const char* bda[] = {"17.11.2013", "22.09.2001"};
		for (size_t i = 0; i < sizeof(bda)/sizeof(bda[0]); i++) {
			help_enc = new EncodeBDA(bda[i]);
			std::cout << "EncodeBDA: " << std::setw(20) << bda[i] << " = " << help_enc->encode() << std::endl;

			delete help_enc;
		}

		std::cout << std::endl;
	}

	// HDA
	{
		const char* hda[] = {"01.01.2001", "31.12.2027"};
		for (size_t i = 0; i < sizeof(hda)/sizeof(hda[0]); i++) {
			help_enc = new EncodeHDA(hda[i]);
			std::cout << "EncodeHDA: " << std::setw(20) << hda[i] << " = " << help_enc->encode() << std::endl;

			delete help_enc;
		}

		std::cout << std::endl;
	}

	// BTI
	{
		const char* bti[] = {"01:01:01", "17:42:09", "23:59:59"};
		for (size_t i = 0; i < sizeof(bti)/sizeof(bti[0]); i++) {
			help_enc = new EncodeBTI(bti[i]);
			std::cout << "EncodeBTI: " << std::setw(20) << bti[i] << " = " << help_enc->encode() << std::endl;

			delete help_enc;
		}

		std::cout << std::endl;
	}

	// HTI
	{
		const char* hti[] = {"01:01:01", "17:42:09", "23:59:59"};
		for (size_t i = 0; i < sizeof(hti)/sizeof(hti[0]); i++) {
			help_enc = new EncodeHTI(hti[i]);
			std::cout << "EncodeHTI: " << std::setw(20) << hti[i] << " = " << help_enc->encode() << std::endl;

			delete help_enc;
		}

		std::cout << std::endl;
	}

	// BDY
	{
		const char* bdy[] = {"Tue", "Thu", "Sun", "Err"};
		for (size_t i = 0; i < sizeof(bdy)/sizeof(bdy[0]); i++) {
			help_enc = new EncodeBDY(bdy[i]);
			std::cout << "EncodeBDY: " << std::setw(20) << bdy[i] << " = " << help_enc->encode() << std::endl;

			delete help_enc;
		}

		std::cout << std::endl;
	}


	// HDY
	{
		const char* hdy[] = {"Mon", "Wed", "Sun", "Err"};
		for (size_t i = 0; i < sizeof(hdy)/sizeof(hdy[0]); i++) {
			help_enc = new EncodeHDY(hdy[i]);
			std::cout << "EncodeHDY: " << std::setw(20) << hdy[i] << " = " << help_enc->encode() << std::endl;

			delete help_enc;
		}

		std::cout << std::endl;
	}

	// TTM
	{
		const char* ttm[] = {"00:00", "05:50", "13:10", "24:00"};
		for (size_t i = 0; i < sizeof(ttm)/sizeof(ttm[0]); i++) {
			help_enc = new EncodeTTM(ttm[i]);
			std::cout << "EncodeTTM: " << std::setw(20) << ttm[i] << " = " << help_enc->encode() << std::endl;

			delete help_enc;
		}

		std::cout << std::endl;
	}

	return 0;
}


