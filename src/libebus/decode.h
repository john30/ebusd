/*
 * Copyright (C) Roland Jax 2014 <roland.jax@liwest.at>
 * crc calculations from http://www.mikrocontroller.net/topic/75698
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

#ifndef LIBEBUS_DECODE_H_
#define LIBEBUS_DECODE_H_

#include <string>

namespace libebus
{


class Decode
{

public:
	Decode(const std::string& data, const std::string& factor = "");
	virtual ~Decode() {}

	virtual std::string decode() = 0;

protected:
	std::string m_data;
	float m_factor;

};


class DecodeHEX : public Decode
{

public:
	DecodeHEX(const std::string& data) : Decode(data) {}
	~DecodeHEX() {}

	std::string decode();

};

class DecodeUCH : public Decode
{

public:
	DecodeUCH(const std::string& data, const std::string& factor)
		: Decode(data, factor) {}
	~DecodeUCH() {}

	std::string decode();

};

class DecodeSCH : public Decode
{

public:
	DecodeSCH(const std::string& data, const std::string& factor)
		: Decode(data, factor) {}
	~DecodeSCH() {}

	std::string decode();

};

class DecodeUIN : public Decode
{

public:
	DecodeUIN(const std::string& data, const std::string& factor)
		: Decode(data, factor) {}
	~DecodeUIN() {}

	std::string decode();

};

class DecodeSIN : public Decode
{

public:
	DecodeSIN(const std::string& data, const std::string& factor)
		: Decode(data, factor) {}
	~DecodeSIN() {}

	std::string decode();

};

class DecodeULG : public Decode
{

public:
	DecodeULG(const std::string& data, const std::string& factor)
		: Decode(data, factor) {}
	~DecodeULG() {}

	std::string decode();

};

class DecodeSLG : public Decode
{

public:
	DecodeSLG(const std::string& data, const std::string& factor)
		: Decode(data, factor) {}
	~DecodeSLG() {}

	std::string decode();

};

class DecodeFLT : public Decode
{

public:
	DecodeFLT(const std::string& data, const std::string& factor)
		: Decode(data, factor) {}
	~DecodeFLT() {}

	std::string decode();

};

class DecodeSTR : public Decode
{

public:
	DecodeSTR(std::string data) : Decode(data) {}
	~DecodeSTR() {}

	std::string decode();

};

class DecodeBCD : public Decode
{

public:
	DecodeBCD(const std::string& data, const std::string& factor)
		: Decode(data, factor) {}
	~DecodeBCD() {}

	std::string decode();

};

class DecodeD1B : public Decode
{

public:
	DecodeD1B(const std::string& data, const std::string& factor)
		: Decode(data, factor) {}
	~DecodeD1B() {}

	std::string decode();

};

class DecodeD1C : public Decode
{

public:
	DecodeD1C(const std::string& data, const std::string& factor)
		: Decode(data, factor) {}
	~DecodeD1C() {}

	std::string decode();

};

class DecodeD2B : public Decode
{

public:
	DecodeD2B(const std::string& data, const std::string& factor)
		: Decode(data, factor) {}
	~DecodeD2B() {}

	std::string decode();

};

class DecodeD2C : public Decode
{

public:
	DecodeD2C(const std::string& data, const std::string& factor)
		: Decode(data, factor) {}
	~DecodeD2C() {}

	std::string decode();

};

class DecodeBDA : public Decode
{

public:
	DecodeBDA(const std::string& data) : Decode(data) {}
	~DecodeBDA() {}

	std::string decode();

};

class DecodeHDA : public Decode
{

public:
	DecodeHDA(const std::string& data) : Decode(data) {}
	~DecodeHDA() {}

	std::string decode();

};

class DecodeBTI : public Decode
{

public:
	DecodeBTI(const std::string& data) : Decode(data) {}
	~DecodeBTI() {}

	std::string decode();

};

class DecodeHTI : public Decode
{

public:
	DecodeHTI(const std::string& data) : Decode(data) {}
	~DecodeHTI() {}

	std::string decode();

};

class DecodeBDY : public Decode
{

public:
	DecodeBDY(const std::string& data) : Decode(data) {}
	~DecodeBDY() {}

	std::string decode();

};

class DecodeHDY : public Decode
{

public:
	DecodeHDY(const std::string& data) : Decode(data) {}
	~DecodeHDY() {}

	std::string decode();

};

class DecodeTTM : public Decode
{

public:
	DecodeTTM(const std::string& data) : Decode(data) {}
	~DecodeTTM() {}

	std::string decode();

};


} //namespace

#endif // LIBEBUS_DECODE_H_
