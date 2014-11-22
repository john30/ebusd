/*
 * Copyright (C) Roland Jax 2012-2014 <ebusd@liwest.at>
 *
 * This file is part of ebusd.
 *
 * ebusd is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * ebusd is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with ebusd. If not, see http://www.gnu.org/licenses/.
 */

#ifndef LIBEBUS_DECODE_H_
#define LIBEBUS_DECODE_H_

#include <string>

using namespace std;

class Decode
{

public:
	Decode(const string& data, const string& factor = "");
	virtual ~Decode() {}

	virtual string decode() = 0;

protected:
	string m_data;
	float m_factor;

};


class DecodeHEX : public Decode
{

public:
	DecodeHEX(const string& data) : Decode(data) {}
	~DecodeHEX() {}

	string decode();

};

class DecodeUCH : public Decode
{

public:
	DecodeUCH(const string& data, const string& factor)
		: Decode(data, factor) {}
	~DecodeUCH() {}

	string decode();

};

class DecodeSCH : public Decode
{

public:
	DecodeSCH(const string& data, const string& factor)
		: Decode(data, factor) {}
	~DecodeSCH() {}

	string decode();

};

class DecodeUIN : public Decode
{

public:
	DecodeUIN(const string& data, const string& factor)
		: Decode(data, factor) {}
	~DecodeUIN() {}

	string decode();

};

class DecodeSIN : public Decode
{

public:
	DecodeSIN(const string& data, const string& factor)
		: Decode(data, factor) {}
	~DecodeSIN() {}

	string decode();

};

class DecodeULG : public Decode
{

public:
	DecodeULG(const string& data, const string& factor)
		: Decode(data, factor) {}
	~DecodeULG() {}

	string decode();

};

class DecodeSLG : public Decode
{

public:
	DecodeSLG(const string& data, const string& factor)
		: Decode(data, factor) {}
	~DecodeSLG() {}

	string decode();

};

class DecodeFLT : public Decode
{

public:
	DecodeFLT(const string& data, const string& factor)
		: Decode(data, factor) {}
	~DecodeFLT() {}

	string decode();

};

class DecodeSTR : public Decode
{

public:
	DecodeSTR(string data) : Decode(data) {}
	~DecodeSTR() {}

	string decode();

};

class DecodeBCD : public Decode
{

public:
	DecodeBCD(const string& data, const string& factor)
		: Decode(data, factor) {}
	~DecodeBCD() {}

	string decode();

};

class DecodeD1B : public Decode
{

public:
	DecodeD1B(const string& data, const string& factor)
		: Decode(data, factor) {}
	~DecodeD1B() {}

	string decode();

};

class DecodeD1C : public Decode
{

public:
	DecodeD1C(const string& data, const string& factor)
		: Decode(data, factor) {}
	~DecodeD1C() {}

	string decode();

};

class DecodeD2B : public Decode
{

public:
	DecodeD2B(const string& data, const string& factor)
		: Decode(data, factor) {}
	~DecodeD2B() {}

	string decode();

};

class DecodeD2C : public Decode
{

public:
	DecodeD2C(const string& data, const string& factor)
		: Decode(data, factor) {}
	~DecodeD2C() {}

	string decode();

};

class DecodeBDA : public Decode
{

public:
	DecodeBDA(const string& data) : Decode(data) {}
	~DecodeBDA() {}

	string decode();

};

class DecodeHDA : public Decode
{

public:
	DecodeHDA(const string& data) : Decode(data) {}
	~DecodeHDA() {}

	string decode();

};

class DecodeBTI : public Decode
{

public:
	DecodeBTI(const string& data) : Decode(data) {}
	~DecodeBTI() {}

	string decode();

};

class DecodeHTI : public Decode
{

public:
	DecodeHTI(const string& data) : Decode(data) {}
	~DecodeHTI() {}

	string decode();

};

class DecodeBDY : public Decode
{

public:
	DecodeBDY(const string& data) : Decode(data) {}
	~DecodeBDY() {}

	string decode();

};

class DecodeHDY : public Decode
{

public:
	DecodeHDY(const string& data) : Decode(data) {}
	~DecodeHDY() {}

	string decode();

};

class DecodeTTM : public Decode
{

public:
	DecodeTTM(const string& data) : Decode(data) {}
	~DecodeTTM() {}

	string decode();

};

#endif // LIBEBUS_DECODE_H_
