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

class Encode
{

public:
	Encode(const std::string& data, const std::string& factor = "");
	virtual ~Encode() {}

	virtual std::string encode() = 0;

protected:
	std::string m_data;
	float m_factor;

};


class EncodeHEX : public Encode
{

public:
	EncodeHEX(const std::string& data) : Encode(data) {}
	~EncodeHEX() {}

	std::string encode();

};

class EncodeUCH : public Encode
{

public:
	EncodeUCH(const std::string& data, const std::string& factor)
		: Encode(data, factor) {}
	~EncodeUCH() {}

	std::string encode();

};

class EncodeSCH : public Encode
{

public:
	EncodeSCH(const std::string& data, const std::string& factor)
		: Encode(data, factor) {}
	~EncodeSCH() {}

	std::string encode();

};

class EncodeUIN : public Encode
{

public:
	EncodeUIN(const std::string& data, const std::string& factor)
		: Encode(data, factor) {}
	~EncodeUIN() {}

	std::string encode();

};

class EncodeSIN : public Encode
{

public:
	EncodeSIN(const std::string& data, const std::string& factor)
		: Encode(data, factor) {}
	~EncodeSIN() {}

	std::string encode();

};

class EncodeULG : public Encode
{

public:
	EncodeULG(const std::string& data, const std::string& factor)
		: Encode(data, factor) {}
	~EncodeULG() {}

	std::string encode();

};

class EncodeSLG : public Encode
{

public:
	EncodeSLG(const std::string& data, const std::string& factor)
		: Encode(data, factor) {}
	~EncodeSLG() {}

	std::string encode();

};

class EncodeFLT : public Encode
{

public:
	EncodeFLT(const std::string& data, const std::string& factor)
		: Encode(data, factor) {}
	~EncodeFLT() {}

	std::string encode();

};

class EncodeSTR : public Encode
{

public:
	EncodeSTR(const std::string& data) : Encode(data) {}
	~EncodeSTR() {}

	std::string encode();

};

class EncodeBCD : public Encode
{

public:
	EncodeBCD(const std::string& data, const std::string& factor)
		: Encode(data, factor) {}
	~EncodeBCD() {}

	std::string encode();

};

class EncodeD1B : public Encode
{

public:
	EncodeD1B(const std::string& data, const std::string& factor)
		: Encode(data, factor) {}
	~EncodeD1B() {}

	std::string encode();

};

class EncodeD1C : public Encode
{

public:
	EncodeD1C(const std::string& data, const std::string& factor)
		: Encode(data, factor) {}
	~EncodeD1C() {}

	std::string encode();

};

class EncodeD2B : public Encode
{

public:
	EncodeD2B(const std::string& data, const std::string& factor)
		: Encode(data, factor) {}
	~EncodeD2B() {}

	std::string encode();

};

class EncodeD2C : public Encode
{

public:
	EncodeD2C(const std::string& data, const std::string& factor)
		: Encode(data, factor) {}
	~EncodeD2C() {}

	std::string encode();

};

class EncodeBDA : public Encode
{

public:
	EncodeBDA(const std::string& data) : Encode(data) {}
	~EncodeBDA() {}

	std::string encode();

};

class EncodeHDA : public Encode
{

public:
	EncodeHDA(const std::string& data) : Encode(data) {}
	~EncodeHDA() {}

	std::string encode();

};

class EncodeBTI : public Encode
{

public:
	EncodeBTI(const std::string& data) : Encode(data) {}
	~EncodeBTI() {}

	std::string encode();

};

class EncodeHTI : public Encode
{

public:
	EncodeHTI(const std::string& data) : Encode(data) {}
	~EncodeHTI() {}

	std::string encode();

};

class EncodeBDY : public Encode
{

public:
	EncodeBDY(const std::string& data) : Encode(data) {}
	~EncodeBDY() {}

	std::string encode();

};

class EncodeHDY : public Encode
{

public:
	EncodeHDY(const std::string& data) : Encode(data) {}
	~EncodeHDY() {}

	std::string encode();

};

class EncodeTTM : public Encode
{

public:
	EncodeTTM(const std::string& data) : Encode(data) {}
	~EncodeTTM() {}

	std::string encode();

};

#endif // LIBEBUS_ENCODE_H_
