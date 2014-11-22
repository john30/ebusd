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

#ifndef LIBEBUS_ENCODE_H_
#define LIBEBUS_ENCODE_H_

#include <string>

using namespace std;

class Encode
{

public:
	Encode(const string& data, const string& factor = "");
	virtual ~Encode() {}

	virtual string encode() = 0;

protected:
	string m_data;
	float m_factor;

};


class EncodeHEX : public Encode
{

public:
	EncodeHEX(const string& data) : Encode(data) {}
	~EncodeHEX() {}

	string encode();

};

class EncodeUCH : public Encode
{

public:
	EncodeUCH(const string& data, const string& factor)
		: Encode(data, factor) {}
	~EncodeUCH() {}

	string encode();

};

class EncodeSCH : public Encode
{

public:
	EncodeSCH(const string& data, const string& factor)
		: Encode(data, factor) {}
	~EncodeSCH() {}

	string encode();

};

class EncodeUIN : public Encode
{

public:
	EncodeUIN(const string& data, const string& factor)
		: Encode(data, factor) {}
	~EncodeUIN() {}

	string encode();

};

class EncodeSIN : public Encode
{

public:
	EncodeSIN(const string& data, const string& factor)
		: Encode(data, factor) {}
	~EncodeSIN() {}

	string encode();

};

class EncodeULG : public Encode
{

public:
	EncodeULG(const string& data, const string& factor)
		: Encode(data, factor) {}
	~EncodeULG() {}

	string encode();

};

class EncodeSLG : public Encode
{

public:
	EncodeSLG(const string& data, const string& factor)
		: Encode(data, factor) {}
	~EncodeSLG() {}

	string encode();

};

class EncodeFLT : public Encode
{

public:
	EncodeFLT(const string& data, const string& factor)
		: Encode(data, factor) {}
	~EncodeFLT() {}

	string encode();

};

class EncodeSTR : public Encode
{

public:
	EncodeSTR(const string& data) : Encode(data) {}
	~EncodeSTR() {}

	string encode();

};

class EncodeBCD : public Encode
{

public:
	EncodeBCD(const string& data, const string& factor)
		: Encode(data, factor) {}
	~EncodeBCD() {}

	string encode();

};

class EncodeD1B : public Encode
{

public:
	EncodeD1B(const string& data, const string& factor)
		: Encode(data, factor) {}
	~EncodeD1B() {}

	string encode();

};

class EncodeD1C : public Encode
{

public:
	EncodeD1C(const string& data, const string& factor)
		: Encode(data, factor) {}
	~EncodeD1C() {}

	string encode();

};

class EncodeD2B : public Encode
{

public:
	EncodeD2B(const string& data, const string& factor)
		: Encode(data, factor) {}
	~EncodeD2B() {}

	string encode();

};

class EncodeD2C : public Encode
{

public:
	EncodeD2C(const string& data, const string& factor)
		: Encode(data, factor) {}
	~EncodeD2C() {}

	string encode();

};

class EncodeBDA : public Encode
{

public:
	EncodeBDA(const string& data) : Encode(data) {}
	~EncodeBDA() {}

	string encode();

};

class EncodeHDA : public Encode
{

public:
	EncodeHDA(const string& data) : Encode(data) {}
	~EncodeHDA() {}

	string encode();

};

class EncodeBTI : public Encode
{

public:
	EncodeBTI(const string& data) : Encode(data) {}
	~EncodeBTI() {}

	string encode();

};

class EncodeHTI : public Encode
{

public:
	EncodeHTI(const string& data) : Encode(data) {}
	~EncodeHTI() {}

	string encode();

};

class EncodeBDY : public Encode
{

public:
	EncodeBDY(const string& data) : Encode(data) {}
	~EncodeBDY() {}

	string encode();

};

class EncodeHDY : public Encode
{

public:
	EncodeHDY(const string& data) : Encode(data) {}
	~EncodeHDY() {}

	string encode();

};

class EncodeTTM : public Encode
{

public:
	EncodeTTM(const string& data) : Encode(data) {}
	~EncodeTTM() {}

	string encode();

};

#endif // LIBEBUS_ENCODE_H_
