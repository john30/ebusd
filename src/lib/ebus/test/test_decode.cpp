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

#include "decode.h"
#include <iostream>
#include <iomanip>

using namespace std;

int main()
{
	Decode* help_dec = NULL;

	cout << endl;

	// HEX
	{
		const char* hex[] = {"53706569636865722020"};
		for (size_t i = 0; i < sizeof(hex)/sizeof(hex[0]); i++) {
			help_dec = new DecodeHEX(hex[i]);
			cout << "DecodeHEX: " << setw(20) << hex[i] << " = " << help_dec->decode() << endl;

			delete help_dec;
		}

		cout << endl;
	}

	// UCH
	{
		const char* uch[] = {"00", "01", "7f", "80", "fe", "ff", "a1"};
		for (size_t i = 0; i < sizeof(uch)/sizeof(uch[0]); i++) {
			help_dec = new DecodeUCH(uch[i], "1.0");
			cout << "DecodeUCH: " << setw(20) << uch[i] << " = " << help_dec->decode() << endl;

			delete help_dec;
		}

		cout << endl;
	}

	// SCH
	{
		const char* sch[] = {"00", "01", "7f", "80", "fe", "ff", "a1"};
		for (size_t i = 0; i < sizeof(sch)/sizeof(sch[0]); i++) {
			help_dec = new DecodeSCH(sch[i], "1.0");
			cout << "DecodeSCH: " << setw(20) << sch[i] << " = " << help_dec->decode() << endl;

			delete help_dec;
		}

		cout << endl;
	}

	// UIN
	{
		const char* uin[] = {"0000", "0001", "7fff", "8000", "fffe", "ffff", "a1b2"};
		for (size_t i = 0; i < sizeof(uin)/sizeof(uin[0]); i++) {
			help_dec = new DecodeUIN(uin[i], "1.0");
			cout << "DecodeUIN: " << setw(20) << uin[i] << " = " << help_dec->decode() << endl;

			delete help_dec;
		}

		cout << endl;
	}

	// SIN
	{
		const char* sin[] = {"0000", "0001", "7fff", "8000", "fffe", "ffff", "a1b2"};
		for (size_t i = 0; i < sizeof(sin)/sizeof(sin[0]); i++) {
			help_dec = new DecodeSIN(sin[i], "1.0");
			cout << "DecodeSIN: " << setw(20) << sin[i] << " = " << help_dec->decode() << endl;

			delete help_dec;
		}

		cout << endl;
	}

	// ULG
	{
		const char* ulg[] = {"00000000", "00000001", "7fffffff", "80000000", "fffffffe", "ffffffff", "a1b2c3d4"};
		for (size_t i = 0; i < sizeof(ulg)/sizeof(ulg[0]); i++) {
			help_dec = new DecodeULG(ulg[i], "1.0");
			cout << "DecodeULG: " << setw(20) << ulg[i] << " = " << help_dec->decode() << endl;

			delete help_dec;
		}

		cout << endl;
	}

	// SLG
	{
		const char* slg[] = {"00000000", "00000001", "7fffffff", "80000000", "fffffffe", "ffffffff", "a1b2c3d4"};
		for (size_t i = 0; i < sizeof(slg)/sizeof(slg[0]); i++) {
			help_dec = new DecodeSLG(slg[i], "1.0");
			cout << "DecodeSLG: " << setw(20) << slg[i] << " = " << help_dec->decode() << endl;

			delete help_dec;
		}

		cout << endl;
	}

	// FLT
	{
		const char* flt[] = {"0000", "081b", "2532", "2689", "0851"};
		for (size_t i = 0; i < sizeof(flt)/sizeof(flt[0]); i++) {
			help_dec = new DecodeFLT(flt[i], "1.0");
			cout << "DecodeFLT: " << setw(20) << flt[i] << " = " << help_dec->decode() << endl;

			delete help_dec;
		}

		cout << endl;
	}

	// STR
	{
		const char* str[] = {"53706569636865722020", "5644363030" };
		for (size_t i = 0; i < sizeof(str)/sizeof(str[0]); i++) {
			help_dec = new DecodeSTR(str[i]);
			cout << "DecodeSTR: " << setw(20) << str[i] << " = " << help_dec->decode() << endl;

			delete help_dec;
		}

		cout << endl;
	}

	// BCD
	{
		const char* bcd[] = {"00", "01", "02", "03", "12", "99"};
		for (size_t i = 0; i < sizeof(bcd)/sizeof(bcd[0]); i++) {
			help_dec = new DecodeBCD(bcd[i], "1.0");
			cout << "DecodeBCD: " << setw(20) << bcd[i] << " = " << help_dec->decode() << endl;

			delete help_dec;
		}

		cout << endl;
	}

	// D1B
	{
		const char* d1b[] = {"00", "01", "7f", "81", "80"};
		for (size_t i = 0; i < sizeof(d1b)/sizeof(d1b[0]); i++) {
			help_dec = new DecodeD1B(d1b[i], "1.0");
			cout << "DecodeD1B: " << setw(20) << d1b[i] << " = " << help_dec->decode() << endl;

			delete help_dec;
		}

		cout << endl;
	}

	// D1C
	{
		const char* d1c[] = {"00", "64", "c8"};
		for (size_t i = 0; i < sizeof(d1c)/sizeof(d1c[0]); i++) {
			help_dec = new DecodeD1C(d1c[i], "1.0");
			cout << "DecodeD1C: " << setw(20) << d1c[i] << " = " << help_dec->decode() << endl;

			delete help_dec;
		}

		cout << endl;
	}

	// D2B
	{
		const char* d2b[] = {"0000", "0100", "ffff", "00ff", "0080", "0180", "ff7f"};
		for (size_t i = 0; i < sizeof(d2b)/sizeof(d2b[0]); i++) {
			help_dec = new DecodeD2B(d2b[i], "1.0");
			cout << "DecodeD2B: " << setw(20) << d2b[i] << " = " << help_dec->decode() << endl;

			delete help_dec;
		}

		cout << endl;
	}

	// D2C
	{
		const char* d2c[] = {"0000", "0100", "ffff", "f0ff", "0080", "0180", "ff7f"};
		for (size_t i = 0; i < sizeof(d2c)/sizeof(d2c[0]); i++) {
			help_dec = new DecodeD2C(d2c[i], "1.0");
			cout << "DecodeD2C: " << setw(20) << d2c[i] << " = " << help_dec->decode() << endl;

			delete help_dec;
		}

		cout << endl;
	}

	// BDA
	{
		const char* bda[] = {"171113", "220901"};
		for (size_t i = 0; i < sizeof(bda)/sizeof(bda[0]); i++) {
			help_dec = new DecodeBDA(bda[i]);
			cout << "DecodeBDA: " << setw(20) << bda[i] << " = " << help_dec->decode() << endl;

			delete help_dec;
		}

		cout << endl;
	}

	// HDA
	{
		const char* hda[] = {"010101", "1f0c1b"};
		for (size_t i = 0; i < sizeof(hda)/sizeof(hda[0]); i++) {
			help_dec = new DecodeHDA(hda[i]);
			cout << "DecodeHDA: " << setw(20) << hda[i] << " = " << help_dec->decode() << endl;

			delete help_dec;
		}

		cout << endl;
	}

	// BTI
	{
		const char* bti[] = {"010101", "174209", "235959"};
		for (size_t i = 0; i < sizeof(bti)/sizeof(bti[0]); i++) {
			help_dec = new DecodeBTI(bti[i]);
			cout << "DecodeBTI: " << setw(20) << bti[i] << " = " << help_dec->decode() << endl;

			delete help_dec;
		}

		cout << endl;
	}

	// HTI
	{
		const char* hti[] = {"010101", "112a09", "173b3b"};
		for (size_t i = 0; i < sizeof(hti)/sizeof(hti[0]); i++) {
			help_dec = new DecodeHTI(hti[i]);
			cout << "DecodeHTI: " << setw(20) << hti[i] << " = " << help_dec->decode() << endl;

			delete help_dec;
		}

		cout << endl;
	}

	// BDY
	{
		const char* bdy[] = {"01", "03", "06", "07"};
		for (size_t i = 0; i < sizeof(bdy)/sizeof(bdy[0]); i++) {
			help_dec = new DecodeBDY(bdy[i]);
			cout << "DecodeBDY: " << setw(20) << bdy[i] << " = " << help_dec->decode() << endl;

			delete help_dec;
		}

		cout << endl;
	}

	// HDY
	{
		const char* hdy[] = {"01", "03", "07", "08"};
		for (size_t i = 0; i < sizeof(hdy)/sizeof(hdy[0]); i++) {
			help_dec = new DecodeHDY(hdy[i]);
			cout << "DecodeHDY: " << setw(20) << hdy[i] << " = " << help_dec->decode() << endl;

			delete help_dec;
		}

		cout << endl;
	}

	// TTM
	{
		const char* ttm[] = {"00", "23", "4f", "90"};
		for (size_t i = 0; i < sizeof(ttm)/sizeof(ttm[0]); i++) {
			help_dec = new DecodeTTM(ttm[i]);
			cout << "DecodeTTM: " << setw(20) << ttm[i] << " = " << help_dec->decode() << endl;

			delete help_dec;
		}

		cout << endl;
	}

	return 0;
}


