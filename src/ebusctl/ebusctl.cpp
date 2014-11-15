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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "appl.h"
#include "port.h"
#include "decode.h"
#include "tcpsocket.h"
#include <iostream>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <cstdlib>
#include <utility>
#include <unistd.h>

using namespace libebus;

Appl& A = Appl::Instance(true);

void define_args()
{
	A.setVersion("ebusctl is part of """PACKAGE_STRING"");

	A.addText(" local commands:\n"
		  "  'scan' scans the bus and identifies the participants\n\n"
		  "  'feed' sends a dump file to a local virtual serial device\n"
		  "        (hint: socat -d -d pty,raw,echo=0 pty,raw,echo=0)\n\n"
		  " remote commands:\n"
		  "   send 'help' to server\n\n"
		  " Options:\n");

	A.addOption("device", "d", OptVal("/dev/ttyUSB60"), dt_string, ot_mandatory,
		    "virtual serial device (/dev/ttyUSB60)");

	A.addOption("file", "f", OptVal("/tmp/ebus_dump.bin"),dt_string, ot_mandatory,
		    "dump file name (/tmp/ebus_dump.bin)");

	A.addOption("time", "t", OptVal(10000), dt_long, ot_mandatory,
		    "delay between 2 bytes in 'us' (10000)\n");

	A.addOption("server", "s", OptVal("localhost"), dt_string, ot_mandatory,
		    "name or ip (localhost)");

	A.addOption("port", "p", OptVal(8888), dt_int, ot_mandatory,
		    "port (8888)\n");
}


template<typename T, size_t N>T * end(T (&ra)[N]) { return ra + N; }

const char *sinit[] = {"02", "04", "05", "06", "08", "09", "0A", "0B", "0C",
		       "0E", "12", "14", "15", "16", "18", "19", "1A", "1B",
		       "1C", "1D", "1E", "20", "21", "22", "23", "24", "25",
		       "26", "27", "28", "29", "2A", "2B", "2C", "2D", "2E",
		       "2F", "32", "34", "35", "36", "38", "39", "3A", "3B",
		       "3C", "3D", "3E", "40", "41", "42", "43", "44", "45",
		       "46", "47", "48", "49", "4A", "4B", "4C", "4D", "4E",
		       "4F", "50", "51", "52", "53", "54", "55", "56", "57",
		       "58", "59", "5A", "5B", "5C", "5D", "5E", "5F", "60",
		       "61", "62", "63", "64", "65", "66", "67", "68", "69",
		       "6A", "6B", "6C", "6D", "6E", "6F", "72", "74", "75",
		       "76", "78", "79", "7A", "7B", "7C", "7D", "7E", "80",
		       "81", "82", "83", "84", "85", "86", "87", "88", "89",
		       "8A", "8B", "8C", "8D", "8E", "8F", "90", "91", "92",
		       "93", "94", "95", "96", "97", "98", "99", "9A", "9B",
		       "9C", "9D", "9E", "9F", "A0", "A1", "A2", "A3", "A4",
		       "A5", "A6", "A7", "A8", "AB", "AC", "AD", "AE", "AF",
		       "B0", "B1", "B2", "B3", "B4", "B5", "B6", "B7", "B8",
		       "B9", "BA", "BB", "BC", "BD", "BE", "BF", "C0", "C1",
		       "C2", "C3", "C4", "C5", "C6", "C7", "C8", "C9", "CA",
		       "CB", "CC", "CD", "CE", "CF", "D0", "D1", "D2", "D3",
		       "D4", "D5", "D6", "D7", "D8", "D9", "DA", "DB", "DC",
		       "DD", "DE", "DF", "E0", "E1", "E2", "E3", "E4", "E5",
		       "E6", "E7", "E8", "E9", "EA", "EB", "EC", "ED", "EE",
		       "EF", "F2", "F4", "F5", "F6", "F8", "F9", "FA", "FB",
		       "FC", "FD"};

static std::vector<std::string> s(sinit, end(sinit));

static std::map<std::string, std::string> manufacturer;

static void addManufacturer()
{
	manufacturer.insert(std::make_pair("06", "Karl Dungs GmbH"));
	manufacturer.insert(std::make_pair("0f", "FH Braunschweig/Wolfenbüttel"));
	manufacturer.insert(std::make_pair("10", "TEM AG für Elektronik Intertem Vertriebs AG"));
	manufacturer.insert(std::make_pair("11", "Lamberti Elektronik"));
	manufacturer.insert(std::make_pair("14", "CEB Compagnie Européenne de Brûleurs S.A."));
	manufacturer.insert(std::make_pair("15", "Landis & Staefa"));
	manufacturer.insert(std::make_pair("16", "FERRO Wärmetechnik GmbH & Co.KG"));
	manufacturer.insert(std::make_pair("17", "MONDIAL electronic Ges.mbH"));
	manufacturer.insert(std::make_pair("18", "Wikon Kommunikationstechnik GmbH"));
	manufacturer.insert(std::make_pair("19", "Wolf GmbH"));
	manufacturer.insert(std::make_pair("20", "RAWE Electronic GmbH"));
	manufacturer.insert(std::make_pair("30", "Satronic AG"));
	manufacturer.insert(std::make_pair("40", "ENCON Electronics"));
	manufacturer.insert(std::make_pair("50", "G. Kromschröder AG"));
	manufacturer.insert(std::make_pair("60", "Eberle Controls GmbH"));
	manufacturer.insert(std::make_pair("65", "EBV Elektronikbau"));
	manufacturer.insert(std::make_pair("75", "Grässlin GmbH & Co.KG"));
	manufacturer.insert(std::make_pair("85", "Motoren und Ventilatoren Landshut GmbH"));
	manufacturer.insert(std::make_pair("95", "SIG Berger Lahr GmbH & Co KG"));
	manufacturer.insert(std::make_pair("c0", "Toby AG"));
	manufacturer.insert(std::make_pair("c5", "Max Weishaupt GmbH"));
	manufacturer.insert(std::make_pair("a5", "Theben Zeitschaltautomatik"));
	manufacturer.insert(std::make_pair("a7", "Thermowatt s.p.a."));
	manufacturer.insert(std::make_pair("b5", "Joh. Vaillant GmbH & Co."));
}

void scanVaillant(TCPSocket* socket, const std::string address)
{
	Decode* help = NULL;
	std::ostringstream sstr;

	for (int i = 24; i < 28; i++) {
		// build message
		std::string message("hex ms ");
		message += address;
		message += "b50901";

		std::stringstream ss;
		ss << i;
		message += ss.str();

		socket->send(message.c_str(), message.size());

		char data[256];
		size_t datalen;

		datalen = socket->recv(data, sizeof(data)-1);
		data[datalen] = '\0';

		std::string item(data);

		help = new DecodeSTR(item.substr(18,18));
		sstr << help->decode();
		delete help;
	}

	std::cout << "   s/n: '" << sstr.str().substr(1,28) << "'   item: '" << sstr.str().substr(7,10) << "'";
}


int main(int argc, char* argv[])
{
	// define arguments and application variables
	define_args();

	// parse arguments
	A.parseArgs(argc, argv);

	if (strcasecmp(A.getArg(0).c_str(), "feed") == 0) {
		std::string dev(A.getOptVal<const char*>("device"));
		Port port(dev, true);

		port.open();
		if(port.isOpen() == true) {
			std::cout << "openPort successful." << std::endl;

			std::fstream file(A.getOptVal<const char*>("file"), std::ios::in | std::ios::binary);

			if(file.is_open() == true) {

				while (file.eof() == false) {
					unsigned char byte = file.get();
					std::cout << std::hex << std::setw(2) << std::setfill('0')
					<< static_cast<unsigned>(byte) << std::endl;

					port.send(&byte, 1);
					usleep(A.getOptVal<long>("time"));
				}

				file.close();
			} else {
				std::cout << "error opening file " << A.getOptVal<const char*>("file") << std::endl;
			}

			port.close();
			if(port.isOpen() == false)
				std::cout << "closePort successful." << std::endl;
		} else {
			std::cout << "error opening device " << A.getOptVal<const char*>("device") << std::endl;
		}

	} else {

		TCPClient* client = new TCPClient();
		TCPSocket* socket = client->connect(A.getOptVal<const char*>("server"), A.getOptVal<int>("port"));

		if (socket != NULL) {

			if (strcasecmp(A.getArg(0).c_str(), "scan") == 0) {
				addManufacturer();

				// send command to all slaves
				for (size_t i = 0; i < s.size(); i++) {
					// build message
					std::string message("hex ms ");
					message += s[i];
					message += "070400";

					socket->send(message.c_str(), message.size());

					char data[1024];
					size_t datalen;

					datalen = socket->recv(data, sizeof(data)-1);
					data[datalen] = '\0';

					// decode answer
					if (strncmp(&data[0], "-", 1) != 0) {
						std::string item(data);

						std::ostringstream ident;
						Decode* help = NULL;

						help = new DecodeSTR(item.substr(18,10));
						ident << help->decode();
						delete help;

						std::cout << s[i] << ":   '" << manufacturer.find(item.substr(16,2))->second
							  << "'   ident: '" << std::setw(5) << std::setfill(' ') << ident.str()
							  << "'   sw: '" << item.substr(28,2)
							  << "." << item.substr(30,2)
							  << "' hw: '" << item.substr(32,2)
							  << "." << item.substr(34,2)
							  << "'";

						if (item.substr(16,2) == "b5")
							scanVaillant(socket, s[i]);

						std::cout << std::endl;
					}

					sleep(2);
				}
			} else {
				// build message
				std::string message(A.getArg(0));
				for (int i = 1; i < A.numArgs(); i++) {
					message += " ";
					message += A.getArg(i);
				}

				socket->send(message.c_str(), message.size());

				char data[1024];
				size_t datalen;

				datalen = socket->recv(data, sizeof(data)-1);
				data[datalen] = '\0';

				std::cout << data;
			}

			delete socket;
		} else {
			std::cout << "error connecting to " << A.getOptVal<const char*>("server")
				  << ":" << A.getOptVal<int>("port") << std::endl;
		}

		delete client;

	}

	return 0;

}

