/*
 * ebusd - daemon for communication with eBUS heating systems.
 * Copyright (C) 2014-2021 John Baier <ebusd@ebusd.eu>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <unistd.h>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <string>
#include <vector>
#include <map>
#include "lib/ebus/message.h"

using namespace ebusd;
using std::cout;
using std::endl;

static bool error = false;

void verify(bool expectFailMatch, string type, string input,
    bool match, string expectStr, string gotStr) {
  if (expectFailMatch) {
    if (match) {
      error = true;
      cout << "  failed " << type << " match >" << input
              << "< error: unexpectedly succeeded" << endl;
    } else {
      cout << "  failed " << type << " match >" << input << "< OK" << endl;
    }
  } else if (match) {
    cout << "  " << type << " match >" << input << "< OK" << endl;
  } else {
    error = true;
    cout << "  " << type << " match >" << input << "< error: got >"
            << gotStr << "<, expected >" << expectStr << "<" << endl;
  }
}

DataFieldTemplates* templates = nullptr;

namespace ebusd {

DataFieldTemplates* getTemplates(const string& filename) {
  if (filename == "") {  // avoid compiler warning
    return templates;
  }
  return templates;
}

result_t loadDefinitionsFromConfigPath(FileReader* reader, const string& filename, bool verbose,
    map<string, string>* defaults, string* errorDescription, bool replace = false) {
  time_t mtime = 0;
  istream* stream = FileReader::openFile(filename, errorDescription, &mtime);
  result_t result;
  if (stream) {
    result = reader->readFromStream(stream, filename, mtime, verbose, defaults, errorDescription);
    delete(stream);
  } else {
    result = RESULT_ERR_NOTFOUND;
  }
  return result;
}


}  // namespace ebusd

int main() {
  // message:   [type],[circuit],name,[comment],[QQ[;QQ]*],[ZZ],[PBSB],[ID],fields...
  // field:     name,part,type[:len][,[divisor|values][,[unit][,[comment]]]]
  // template:  name,type[:len][,[divisor|values][,[unit][,[comment]]]]
  // condition: name,circuit,messagename,[comment],[fieldname],[ZZ],values
  // "message", "decoded", "master", "slave", "flags"
  unsigned int baseLine = __LINE__+1;
  string checks[][5] = {
    {"date,HDA:3,,,Datum", "", "", "", "template"},
    {"bdate:date,BDA,,,Datum", "", "", "", "template"},
    {"time,VTI,,,", "", "", "", "template"},
    {"btime:time,BTI,,,Uhrzeit", "", "", "", "template"},
    {"dcfstate,UCH,0=nosignal;1=ok;2=sync;3=valid,,", "", "", "", "template"},
    {"temp,D2C,,°C,Temperatur", "", "", "", "template"},
    {"temp1,D1C,,°C,Temperatur", "", "", "", "template"},
    {"temp2,D2B,,°C,Temperatur", "", "", "", "template"},
    {"power,UCH,,kW", "", "", "", "template"},
    {"sensor,UCH,0=ok;85=circuit;170=cutoff,,Fühlerstatus", "", "", "", "template"},
    {"sensorc,UCH,=85,,Fühlerstatus", "", "", "", "template"},
    {"pumpstate,UCH,0=off;1=on;2=overrun,,Pumpenstatus", "", "", "", "template"},
    {"tempsensor,temp;sensor,,Temperatursensor", "", "", "", "template"},
    {"tempsensorc,temp;sensorc,,Temperatursensor", "", "", "", "template"},
    {"r,cir,Status01,VL/RL/AussenTemp/VLWW/SpeicherTemp/Status,,08,B511,01,,,temp1;temp1;temp2;temp1;temp1;pumpstate", "28.0;24.0;4.938;35.0;41.0;4", "ff08b5110101", "093830f00446520400ff", "d"},
    {"r,message circuit,message name,message comment,,25,B509,0d2800,,,tempsensor", "temp=-14.00 Temperatursensor [Temperatur];sensor=ok [Fühlerstatus]", "ff25b509030d2800", "0320ff00", "D"},
    {"r,message circuit,message name,message comment,,25,B509,0d2800,,,tempsensor,,field unit,field comment", "temp=-14.00 field unit [field comment];sensor=ok [Fühlerstatus]", "ff25b509030d2800", "0320ff00", "D"},
    {"r,message circuit,message name,message comment,,25,B509,0d2800,,,tempsensor,,field unit,field comment", "\n     \"temp\": {\"value\": -14.00},\n     \"sensor\": {\"value\": \"ok\"}", "ff25b509030d2800", "0320ff00", "j"},
    {"r,message circuit,message name,message comment,,25,B509,0d2800,,,tempsensor,,field unit,field comment", "\n     \"temp\": {\"value\": -14.00, \"unit\": \"field unit\", \"comment\": \"field comment\"},\n" "     \"sensor\": {\"value\": \"ok\", \"comment\": \"Fühlerstatus\"}", "ff25b509030d2800", "0320ff00", "J"},
    {"r,message circuit,message name,message comment,,25,B509,0d2800,,,temp,,field unit,field comment,,,sensor", "temp=-14.00 field unit [field comment];sensor=ok [Fühlerstatus]", "ff25b509030d2800", "0320ff00", "D"},
    {"r,message circuit,message name,message comment,,25,B509,0d2800,,,D2C,,°C,Temperatur,,,sensor", "\n     \"0\": {\"name\": \"\", \"value\": -14.00},\n     \"1\": {\"name\": \"sensor\", \"value\": \"ok\"}", "ff25b509030d2800", "0320ff00", "j"},
    {"r,cir,name,,,25,B509,0d2800,,,tempsensorc", "-14.00", "ff25b509030d2800", "0320ff55", ""},
    {"r,cir,name,,,25,B509,0d28,,m,sensorc,,,,,,temp", "-14.00", "ff25b509030d2855", "0220ff", ""},
    {"u,cir,first,,,fe,0700,,x,,bda", "26.10.2014", "fffe07000426100614", "00", "p"},
    {"u,broadcast,hwStatus,,,fe,b505,27,,,UCH,,,,,,UCH,,,,,,UCH,,,", "0;19;0", "10feb505042700130097", "00", ""},
    {"u,broadcast,datetime,Datum/Uhrzeit,,fe,0700,,outsidetemp,,temp2,,°C,Aussentemperatur,time,,btime,,,,date,,BDA,,,Datum", "outsidetemp=14.500 °C [Aussentemperatur];time=12:25:01 [Uhrzeit];date=01.05.2017 [Datum]", "10fe070009800e01251201050017", "", "D"},
    {"u,broadcast,datetime,Datum Uhrzeit,,fe,0700,,,,temp2;btime;bdate", "temp2=14.500 °C [Temperatur];time=12:25:01 [Uhrzeit];date=01.05.2017 [Datum]", "10fe070009800e01251201050017", "", "D"},
    {"w,cir,first,,,15,b509,0400,date,,bda", "26.10.2014", "ff15b50906040026100614", "00", ""},
    {"w,cir,first,,,15,b509", "", "ff15b50900", "00", ""},
    {"*w,,,,,,b505,2d", "", "", "", ""},
    {"w,cir,offset,,,50,,,,,temp", "0.50", "ff50b505042d080000", "00", "kd"},
    {"r,ehp,time,,,08,b509,0d2800,,,time", "15:00:17", "ff08b509030d2800", "0311000f", "d"},
    {"r,ehp,time,,,08;10,b509,0d2800,,,time", "", "", "", "c"},
    {"r,ehp,time,,,08;09,b509,0d2800,,,time", "15:00:17", "ff08b509030d2800", "0311000f", "d*"},
    {"r,ehp,date,,,08,b509,0d2900,,,date", "23.11.2014", "ff08b509030d2900", "03170b0e", "d"},
    {"r,700,date,,,15,b524,020000003400,,,IGN:4,,,,,,date", "23.11.2015", "ff15b52406020000003400", "0703003400170b0f", "d"},
    {"", "23.11.2015", "ff15b52406020000003400", "0703003400170b0f", "kd"},
    {"r,700,time,,,15,b524,030000003500,,,IGN:4,,,,,,HTI", "12:29:06", "ff15b52406030000003500", "07030035000c1d06", "d"},
    {"", "12:29:06", "ff15b52406030000003500", "07030035000c1d06", "kd"},
    {"r,700,mupd,,,15,b524,030000000100,,m,UCH,,,,,,HTI", "1;12:29:07", "ff15b5240703000000010001", "030c1d07", "d"},
    {"", "2;12:29:07", "ff15b5240703000000010002", "030c1d07", "kdu"},
    {"", "2;12:29:07", "ff15b5240703000000010002", "030c1d07", "kdU"},
    {"w,700,date,,,15,b524,020000003400,,,date", "23.11.2015", "ff15b52409020000003400170b0f", "00", ""},
    {"r,ehp,error,,,08,b509,0d2800,index,m,UCH,,,,,,time", "3;15:00:17", "ff08b509040d280003", "0311000f", "di"},
    {"r,ehp,error,,,08,b509,0d2800,index,m,UCH,,,,,,time", "index=3;time=15:00:17", "ff08b509040d280003", "0311000f", "D"},
    {"u,ehp,ActualEnvironmentPower,Energiebezug,,08,B509,29BA00,,s,IGN:2,,,,,s,power", "8", "1008b5090329ba00", "03ba0008", "p"},
    {"uw,ehp,test,Test,,08,B5de,ab,,,power,,,,,s,hex:1", "8;39", "1008b5de02ab08", "0139", "p"},
    {"u,ehp,hwTankTemp,Speichertemperatur IST,,25,B509,290000,,,IGN:2,,,,,,tempsensor", "", "", "", "M"},
    {"", "55.50;ok", "1025b50903290000", "050000780300", "kd"},
    {"r,ehp,datetime,Datum Uhrzeit,,50,B504,00,,,dcfstate,,,,time,,BTI,,,,date,,BDA,,,,temp,,temp2", "valid;08:24:51;31.12.2014;-0.875", "1050b5040100", "0a035124083112031420ff", "d" },
    {"r,ehp,bad,invalid pos,,50,B5ff,000102,,m,HEX:8;tempsensor;tempsensor;tempsensor;tempsensor;power;power,,,", "", "", "", "c" },
    {"r,ehp,bad,invalid pos,,50,B5ff,,,s,HEX:8;tempsensor;tempsensor;tempsensor;tempsensor;tempsensor;power;power,,,", "", "", "", "c" },
    {"r,ehp,ApplianceCode,,,08,b509,0d4301,,,UCH,", "9", "ff08b509030d4301", "0109", "d" },
    {"*r,ehp,,,,08,b509,0d", "", "", "", "" },
    {"*w,ehp,,,,08,b509,0e", "", "", "", "" },
    {"*[brinetowater],ehp,ApplianceCode,,,,4;6;8;9;10", "", "", "", "" },
    {"[airtowater]r,ehp,notavailable,,,,,0100,,,uch", "1", "", "", "kc" },
    {"[brinetowater]r,ehp,available,,,,,0100,,,uch", "1", "ff08b509030d0100", "0101", "kd" },
    {"r,,x,,,,,\"6800\",,,UCH,,,bit0=\"comment, continued comment", "", "", "", "c" },
    {"r,,x,,,,,\"6800\",,,UCH,,\"\",\"bit0=\"comment, continued comment\"", "=1 [bit0=\"comment, continued comment]", "ff08b509030d6800", "0101", "D" },
    {"r,ehp,multi,,,,,0001:5;0002;0003,longname,,STR:15", "ABCDEFGHIJKLMNO", "ff08b509030d0001;ff08b509030d0003;ff08b509030d0002", "054142434445;054b4c4d4e4f;05464748494a", "dC" },
    {"r,ehp,multi,,,,,01;02;03,longname,,STR:15", "ABCDEFGHIJKLMNO", "ff08b509020d01;ff08b509020d03;ff08b509020d02", "084142434445464748;054b4c4d4e4f;02494a", "dC" },
    {"w,ehp,multi,,,,,01:8;02:2;03,longname,,STR:15", "ABCDEFGHIJKLMNO", "ff08b5090a0e014142434445464748;ff08b509040e02494a;ff08b509070e034b4c4d4e4f", "00;00;00", "dC" },
    {"w,ehp,multi,,,,,01:8;02:2;0304,longname,,STR:15", "ABCDEFGHIJKLMNO", "ff08b5090a0e014142434445464748;ff08b509040e02494a;ff08b509070e034b4c4d4e4f", "00;00;00", "cC" },
    {"r,ehp,scan,chained scan,,08,B509,24:9;25;26;27,,,IGN,,,,id4,,STR:28", "21074500100027790000000000N8", "ff08b5090124;ff08b5090125;ff08b5090126;ff08b5090127", "09003231303734353030;09313030303237373930;09303030303030303030;024E38", "dC" },
    {"r,,x,,,,,6900,,,UCH,10,bar,,Bit7,,BI7:1,0=B70;1=B71,,,Bit6,,BI6:1,0=B60;1=B61", "1.9;B71;B61", "ff08b509030d6900", "03138040", "d" },
    {"r,,x,,,,,6900,,,UCH,10,bar,,Bit7,,BI7:1,0=B70;1=B71,,,Bit6,,BI6:1,0=B60;1=B61", "1.9;B71;B60", "ff08b509030d6900", "0313ffbf", "d" },
    {"r,,x,,,,,6900,,,UCH,10,bar,,Bit7,,BI7:1,0=B70;1=B71,,,Bit6,,BI6:1,0=B60;1=B61", "1.9;B70;B61", "ff08b509030d6900", "03137fff", "d" },
    {"r,,x,,,,,6900,,,UCH,10,bar,,Bit7,,BI7:1,0=B70;1=B71,,,Bit6,,BI6:1,0=B60;1=B61", "1.9;B70;B60", "ff08b509030d6900", "03137fbf", "d" },
    {"r,,x,,,,,6a00,,,UCH,10,bar,,Bit6,,BI6:1,0=B60;1=B61,,,Bit7,,BI7:1,0=B70;1=B71", "1.9;B61;B71", "ff08b509030d6a00", "0213ff", "d" },
    {"r,,x,,,,,6a00,,,UCH,10,bar,,Bit6,,BI6:1,0=B60;1=B61,,,Bit7,,BI7:1,0=B70;1=B71", "1.9;B60;B71", "ff08b509030d6a00", "0213bf", "d" },
    {"r,,x,,,,,6a00,,,UCH,10,bar,,Bit6,,BI6:1,0=B60;1=B61,,,Bit7,,BI7:1,0=B70;1=B71", "1.9;B61;B70", "ff08b509030d6a00", "02137f", "d" },
    {"r,,x,,,,,6a00,,,UCH,10,bar,,Bit6,,BI6:1,0=B60;1=B61,,,Bit7,,BI7:1,0=B70;1=B71", "1.9;B60;B70", "ff08b509030d6a00", "02133f", "d" },
    {"w,,x,,,,,6900,,,UCH,10,bar,,Bit7,,BI7:1,0=B70;1=B71,,,Bit6,,BI6:1,0=B60;1=B61", "1.9;B71;B61", "ff08b509060e6900138040", "00", "di" },
    {"w,,x,,,,,6900,,,UCH,10,bar,,Bit7,,BI7:1,0=B70;1=B71,,,Bit6,,BI6:1,0=B60;1=B61", "1.9;B71;B60", "ff08b509060e6900138000", "00", "di" },
    {"w,,x,,,,,6900,,,UCH,10,bar,,Bit7,,BI7:1,0=B70;1=B71,,,Bit6,,BI6:1,0=B60;1=B61", "1.9;B70;B61", "ff08b509060e6900130040", "00", "di" },
    {"w,,x,,,,,6900,,,UCH,10,bar,,Bit7,,BI7:1,0=B70;1=B71,,,Bit6,,BI6:1,0=B60;1=B61", "1.9;B70;B60", "ff08b509060e6900130000", "00", "di" },
    {"w,,x,,,,,6a00,,,UCH,10,bar,,Bit6,,BI6:1,0=B60;1=B61,,,Bit7,,BI7:1,0=B70;1=B71", "1.9;B61;B71", "ff08b509050e6a0013c0", "00", "di" },
    {"w,,x,,,,,6a00,,,UCH,10,bar,,Bit6,,BI6:1,0=B60;1=B61,,,Bit7,,BI7:1,0=B70;1=B71", "1.9;B60;B71", "ff08b509050e6a001380", "00", "di" },
    {"w,,x,,,,,6a00,,,UCH,10,bar,,Bit6,,BI6:1,0=B60;1=B61,,,Bit7,,BI7:1,0=B70;1=B71", "1.9;B61;B70", "ff08b509050e6a001340", "00", "di" },
    {"w,,x,,,,,6a00,,,UCH,10,bar,,Bit6,,BI6:1,0=B60;1=B61,,,Bit7,,BI7:1,0=B70;1=B71", "1.9;B60;B70", "ff08b509050e6a001300", "00", "di" },
    {"w,,x,,,,,,,,IGN:1,,,,b0,,BI0:1,,,,b1,,BI1:1,,,,b2,,BI2:1,,,,,,IGN:1,,,,c0,,BI0:1,,,,c1,,BI1:1,,,,c2,,BI2:1", "1;1;1;0;0;0", "ff08b509050e00070000", "00", "di" },
    {"w,,x,,,,,,,,IGN:1,,,,b0,,BI0:1,,,,b1,,BI1:1,,,,b2,,BI2:1,,,,,,IGN:1,,,,c0,,BI0:1,,,,c1,,BI1:1,,,,c2,,BI2:1", "1;0;0;0;0;1", "ff08b509050e00010004", "00", "di" },
    {"w,,x,,,,,,,,IGN:1,,,,b0,,BI0:1,,,,b1,,BI1:1,,,,b2,,BI2:1,,,,,,IGN:1,,,,c0,,BI0:1,,,,c1,,BI1:1,,,,c2,,BI2:1", "0;0;1;0;1;1", "ff08b509050e00040006", "00", "di" },
    {"w,,x,,,,,,b0,,BI0:1,,,,b1,,BI1:1,,,,b2,,BI2:6,,,,c0,,BI0:1,,,,c1,,BI1:1,,,,c2,,BI2:1", "1;1;1;0;0;0", "ff08b509030e0700", "00", "di" },
    {"w,,x,,,,,,b0,,BI0:1,,,,b1,,BI1:1,,,,b2,,BI2:6,,,,c0,,BI0:1,,,,c1,,BI1:1,,,,c2,,BI2:1", "1;0;0;0;0;1", "ff08b509030e0104", "00", "di" },
    {"w,,x,,,,,,b0,,BI0:1,,,,b1,,BI1:1,,,,b2,,BI2:6,,,,c0,,BI0:1,,,,c1,,BI1:1,,,,c2,,BI2:1", "0;0;1;0;1;1", "ff08b509030e0406", "00", "di" },
    {"r,470,ccTimer.Monday,,,15,B515,0002,,,IGN:1,,,,from,,TTM", "", "", "", "M"},
    {"w,470,ccTimer.Monday,,,10,B515,0002,from,,TTM", "", "", "", "kM*"},
    {"", "19:00", "3115b515020002", "080272", "kd"},
    {"", "19:00", "3110b51503000272", "00", "kd"},
    {"*r,cir*cuit#level,na*me,com*ment,ff,75,b509,0d", "", "", "", ""},
    {"r,CIRCUIT,NAME,COMMENT,,,,0100,field,,UCH", "r,cirCIRCUITcuit,naNAMEme,comCOMMENTment,ff,75,b509,0d0100,field,s,UCH,,,: field=42", "ff75b509030d0100", "012a", "DN"},
    {"r,CIRCUIT,NAME,COMMENT,,,,0100,field,,UCH",
        // "\"naNAMEme\": {r,cirCIRCUITcuit,naNAMEme,comCOMMENTment,ff,75,b509,0d0100,field,s,UCH,,,: field=42"
      "\n"
      "   \"naNAMEme\": {\n"
      "    \"name\": \"naNAMEme\",\n"
      "    \"passive\": false,\n"
      "    \"write\": false,\n"
      "    \"lastup\": *,\n"
      "    \"qq\": 255,\n"
      "    \"zz\": 117,\n"
      "    \"id\": [181, 9, 13, 1, 0],\n"
      "    \"fields\": {\n"
      "     \"0\": {\"name\": \"field\", \"value\": 42}\n"
      "    },\n"
      "    \"fielddefs\": [\n"
      "     { \"name\": \"field\", \"slave\": true, \"type\": \"UCH\", \"isbits\": false, \"length\": 1, \"unit\": \"\", \"comment\": \"\"}\n"
      "    ]\n"
      "   }: \n"
      "     \"field\": {\"value\": 42}", "ff75b509030d0100", "012a", "jN"},
  };
  templates = new DataFieldTemplates();
  unsigned int lineNo = 0;
  istringstream dummystr("#");
  string errorDescription;
  vector<string> row;
  templates->readLineFromStream(&dummystr, __FILE__, false, &lineNo, &row, &errorDescription, false, nullptr, nullptr);
  lineNo = 0;
  MessageMap* messages = new MessageMap("");
  dummystr.clear();
  dummystr.str("#");
  messages->readLineFromStream(&dummystr, __FILE__, false, &lineNo, &row, &errorDescription, false, nullptr, nullptr);
  vector< vector<string> > defaultsRows;
  Message* message = nullptr;
  vector<MasterSymbolString*> mstrs;
  vector<SlaveSymbolString*> sstrs;
  mstrs.resize(1);
  sstrs.resize(1);
  for (unsigned int i = 0; i < sizeof(checks) / sizeof(checks[0]); i++) {
    string* check = checks[i];
    string inputStr = check[1];
    string flags = check[4];
    bool isTemplate = flags == "template";
    bool keepMessages = flags.find('k') != string::npos;
    bool checkUpdateTime = flags.find('u') != string::npos;
    bool checkSameChangeTime = flags.find('U') != string::npos;
    bool onlyMap = flags.find('M') != string::npos;
    bool failedCreate = flags.find('c') != string::npos;
    bool isChain = flags.find('C') != string::npos;
    bool decodeJson = flags.find('j') != string::npos || flags.find('J') != string::npos;
    bool decodeVerbose = flags.find('D') != string::npos || flags.find('J') != string::npos;
    bool withMessageDump = flags.find('N') != string::npos;
    bool decode = decodeJson || decodeVerbose || (flags.find('d') != string::npos);
    bool failedPrepare = flags.find('p') != string::npos;
    bool failedPrepareMatch = flags.find('P') != string::npos;
    bool multi = flags.find('*') != string::npos;
    bool withInput = flags.find('i') != string::npos;
    result_t result = RESULT_EMPTY;
    istringstream isstr(check[0]);
    lineNo = baseLine + i;
    cout << "line " << (lineNo+1) << " ";
    if (isTemplate) {
      result = templates->readLineFromStream(&isstr, __FILE__, false, &lineNo, &row, &errorDescription, false, nullptr, nullptr);
      if (result != RESULT_OK) {
        cout << "\"" << check[0] << "\": template read error: " << getResultCode(result) << ", " << errorDescription
            << endl;
        error = true;
      }
      cout << "\"" << check[0] << "\": template read OK" << endl;
      continue;
    }
    if (!keepMessages) {
      messages->clear();
    }
    if (isstr.peek() == '*') {
      // store defaults or condition
      result = messages->readLineFromStream(&isstr, __FILE__, false, &lineNo, &row, &errorDescription, false, nullptr, nullptr);
      if (result != RESULT_OK) {
        cout << "\"" << check[0] << "\": default read error: " << getResultCode(result) << ", " << errorDescription << endl;
        error = true;
        continue;
      }
      cout << "\"" << check[0] << "\": default read OK" << endl;
      continue;
    }
    if (isChain) {
      size_t pos = 0;
      string token;
      istringstream stream(check[2]);
      while (getline(stream, token, VALUE_SEPARATOR)) {
        if (pos >= mstrs.size()) {
          mstrs.resize(pos+1);
        } else if (mstrs[pos] != nullptr) {
          delete mstrs[pos];
        }
        mstrs[pos] = new MasterSymbolString();
        result = mstrs[pos]->parseHex(token);
        if (result != RESULT_OK) {
          cout << "\"" << check[0] << "\": parse \"" << token << "\" error: " << getResultCode(result) << endl;
          error = true;
          break;
        }
        pos++;
      }
      pos = 0;
      stream.str(check[3]);
      stream.clear();
      while (getline(stream, token, VALUE_SEPARATOR)) {
        if (pos >= sstrs.size()) {
          sstrs.resize(pos+1);
        } else if (sstrs[pos] != nullptr) {
          delete sstrs[pos];
        }
        sstrs[pos] = new SlaveSymbolString();
        result = sstrs[pos]->parseHex(token);
        if (result != RESULT_OK) {
          cout << "\"" << check[0] << "\": parse \"" << token << "\" error: " << getResultCode(result) << endl;
          error = true;
          break;
        }
        pos++;
      }
      if (result != RESULT_OK) {
        error = true;
        continue;
      }
    } else {
      if (mstrs[0] != nullptr) {
        delete mstrs[0];
      }
      mstrs[0] = new MasterSymbolString();
      result = mstrs[0]->parseHex(check[2]);
      if (result != RESULT_OK) {
        cout << "\"" << check[0] << "\": parse \"" << check[2] << "\" error: " << getResultCode(result) << endl;
        error = true;
        continue;
      }
      if (sstrs[0] != nullptr) {
        delete sstrs[0];
      }
      sstrs[0] = new SlaveSymbolString();
      result = sstrs[0]->parseHex(check[3]);
      if (result != RESULT_OK) {
        cout << "\"" << check[0] << "\": parse \"" << check[3] << "\" error: " << getResultCode(result) << endl;
        error = true;
        continue;
      }
    }

    if (isstr.peek() == EOF) {
      message = messages->find(*mstrs[0]);
      if (message == nullptr) {
        cout << "\"" << check[2] << "\": find error: nullptr" << endl;
        error = true;
        continue;
      }
      cout << "\"" << check[2] << "\": find OK" << endl;
    } else {
      result = messages->readLineFromStream(&isstr, __FILE__, false, &lineNo, &row, &errorDescription, false, nullptr, nullptr);
      if (failedCreate) {
        if (result == RESULT_OK) {
          cout << "\"" << check[0] << "\": failed create error: unexpectedly succeeded" << endl;
          error = true;
        } else {
          cout << "\"" << check[0] << "\": failed create OK" << endl;
        }
        continue;
      }
      if (result != RESULT_OK) {
        cout << "\"" << check[0] << "\": create error: " << getResultCode(result) << ", " << errorDescription << endl;
        error = true;
        continue;
      }
      if (messages->size() == 0) {
        cout << "\"" << check[0] << "\": create error: nullptr" << endl;
        error = true;
        continue;
      }
      if (multi && messages->size() == 1) {
        cout << "\"" << check[0] << "\": create error: single message instead of multiple" << endl;
        error = true;
        continue;
      }
      if (!multi && messages->size() > 1) {
        cout << "\"" << check[0] << "\": create error: multiple messages instead of single" << endl;
        error = true;
        continue;
      }
      cout << "\"" << check[0] << "\": create OK" << endl;
      if (onlyMap) {
        continue;
      }
      deque<Message*> msgs;
      messages->findAll("", "", "*", false, true, true, true, true, false, 0, 0, false, &msgs);
      if (msgs.empty()) {
        message = nullptr;
        cout << "\"" << check[0] << "\": create error: message not found" << endl;
        error = true;
        continue;
      }
      message = *msgs.begin();
      Message* foundMessage = messages->find(*mstrs[0], false, true, true, true, false);
      if (foundMessage == message) {
        cout << "  find OK" << endl;
      } else if (foundMessage == nullptr) {
        cout << "  find error: message not found by master " << mstrs[0]->getStr() << endl;
        error = true;
        continue;
      } else {
        cout << "  find error: different" << endl;
        error = true;
      }
    }

    if (message->isPassive() || decode) {
      time_t lastUpdateTime = message->getLastUpdateTime();
      time_t lastChangeTime = message->getLastChangeTime();
      if (checkUpdateTime || checkSameChangeTime) {
        sleep(2);
      }
      for (size_t index = 0; index < message->getCount(); index++) {
        message->storeLastData(*mstrs[index], *sstrs[index]);
      }
      ostringstream output;
      if (withMessageDump) {
        if (decodeJson) {
          message->decodeJson(false, false, false, OF_JSON|OF_DEFINTION, &output);
          string str = output.str();
          size_t start = str.find("\"lastup\": ");
          if (start != string::npos) {
            start += 10;
            size_t end = str.find(",", start);
            if (end != string::npos) {
              str = str.substr(0, start)+"*"+str.substr(end);
            }
          }
          output.str("");
          output << str;
        } else {
          message->dump(nullptr, true, &output);
        }
        output << ": ";
      }
      result = message->decodeLastData(false, nullptr, -1,
          (decodeVerbose?OF_NAMES|OF_UNITS|OF_COMMENTS:0)|(decodeJson?OF_NAMES|OF_JSON:0), &output);
      if (result != RESULT_OK) {
        cout << "  \"" << check[2] << "\" / \"" << check[3] << "\": decode error " << (message->isWrite() ? "write: " : "read: ")
            << getResultCode(result) << endl;
        error = true;
        continue;
      }
      cout << "  \"" << check[2] << "\" / \"" << check[3] <<  "\": decode OK" << endl;
      string outStr = output.str();
      bool match = inputStr == output.str();
      verify(false, "decode", check[2] + "/" + check[3], match, inputStr, output.str());
      if (checkUpdateTime || checkSameChangeTime) {
        time_t time = message->getLastUpdateTime();
        if (time == lastUpdateTime) {
          cout << "  update time error: not updated" << endl;
        } else {
          cout << "  update time OK" << endl;
        }
        time = message->getLastChangeTime();
        if (checkSameChangeTime) {
          if (time != lastChangeTime) {
            cout << "  same change time error: unexpectedly updated" << endl;
          } else {
            cout << "  same change time OK" << endl;
          }
        } else {
          if (time == lastChangeTime) {
            cout << "  change time error: not updated" << endl;
          } else {
            cout << "  change time OK" << endl;
          }
        }
      }
    }
    if (!message->isPassive() && (withInput || !decode)) {
      istringstream input(inputStr);
      MasterSymbolString writeMstr;
      result = message->prepareMaster(0, 0xff, SYN, UI_FIELD_SEPARATOR, &input, &writeMstr);
      if (failedPrepare) {
        if (result == RESULT_OK) {
          cout << "  \"" << inputStr << "\": failed prepare error: unexpectedly succeeded" << endl;
          error = true;
        } else {
          cout << "  \"" << inputStr << "\": failed prepare OK" << endl;
        }
        continue;
      }

      if (result != RESULT_OK) {
        cout << "  \"" << inputStr << "\": prepare error: " << getResultCode(result) << endl;
        error = true;
        continue;
      }
      cout << "  \"" << inputStr << "\": prepare OK" << endl;

      bool match = writeMstr == *mstrs[0];
      verify(failedPrepareMatch, "prepare", inputStr, match, mstrs[0]->getStr(), writeMstr.getStr());
    }
  }

  delete templates;
  delete messages;
  for (vector<MasterSymbolString*>::iterator it = mstrs.begin(); it != mstrs.end(); it++) {
    delete *it;
  }
  for (vector<SlaveSymbolString*>::iterator it = sstrs.begin(); it != sstrs.end(); it++) {
    delete *it;
  }
  return error ? 1 : 0;
}
