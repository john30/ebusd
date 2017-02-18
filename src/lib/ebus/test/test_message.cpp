/*
 * ebusd - daemon for communication with eBUS heating systems.
 * Copyright (C) 2014-2017 John Baier <ebusd@ebusd.eu>
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

#include <iostream>
#include <fstream>
#include <iomanip>
#include <string>
#include <vector>
#include <map>
#include "lib/ebus/message.h"

using namespace ebusd;

void verify(bool expectFailMatch, string type, string input,
    bool match, string expectStr, string gotStr) {
  if (expectFailMatch) {
    if (match) {
      cout << "  failed " << type << " match >" << input
              << "< error: unexpectedly succeeded" << endl;
    } else {
      cout << "  failed " << type << " match >" << input << "< OK" << endl;
    }
  } else if (match) {
    cout << "  " << type << " match >" << input << "< OK" << endl;
  } else {
    cout << "  " << type << " match >" << input << "< error: got >"
            << gotStr << "<, expected >" << expectStr << "<" << endl;
  }
}

DataFieldTemplates* templates = NULL;

namespace ebusd {
DataFieldTemplates* getTemplates(const string filename) {
  if (filename == "") {  // avoid compiler warning
    return templates;
  }
  return templates;
}
}

int main() {
  // message:   [type],[circuit],name,[comment],[QQ[;QQ]*],[ZZ],[PBSB],[ID],fields...
  // field:     name,part,type[:len][,[divisor|values][,[unit][,[comment]]]]
  // template:  name,type[:len][,[divisor|values][,[unit][,[comment]]]]
  // condition: name,circuit,messagename,[comment],[fieldname],[ZZ],values
  string checks[][5] = {
    // "message", "decoded", "master", "slave", "flags"
    {"date,HDA:3,,,Datum", "", "", "", "template"},
    {"time,VTI,,,", "", "", "", "template"},
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
    {"r,,Status01,VL/RL/AussenTemp/VLWW/SpeicherTemp/Status,,08,B511,01,,,temp1;temp1;temp2;temp1;temp1;pumpstate",
        "28.0;24.0;4.938;35.0;41.0;4", "ff08b5110101", "093830f00446520400ff", "d"},
    {"r,message circuit,message name,message comment,,25,B509,0d2800,,,tempsensor",
        "temp=-14.00 Temperatursensor [Temperatur];sensor=ok [Fühlerstatus]", "ff25b509030d2800", "0320ff00", "mD"},
    {"r,message circuit,message name,message comment,,25,B509,0d2800,,,tempsensor,,field unit,field comment",
        "temp=-14.00 field unit [field comment];sensor=ok [Fühlerstatus]", "ff25b509030d2800", "0320ff00", "mD"},
    {"r,message circuit,message name,message comment,,25,B509,0d2800,,,tempsensor,,field unit,field comment",
        "\n    \"temp\": {\"value\": -14.00},\n    \"sensor\": {\"value\": \"ok\"}", "ff25b509030d2800", "0320ff00",
        "mj"},
    {"r,message circuit,message name,message comment,,25,B509,0d2800,,,tempsensor,,field unit,field comment",
        "\n    \"temp\": {\"value\": -14.00, \"unit\": \"field unit\", \"comment\": \"field comment\"},\n"
        "    \"sensor\": {\"value\": \"ok\", \"comment\": \"Fühlerstatus\"}", "ff25b509030d2800", "0320ff00", "mJ"},
    {"r,message circuit,message name,message comment,,25,B509,0d2800,,,temp,,field unit,field comment,,,sensor",
        "temp=-14.00 field unit [field comment];sensor=ok [Fühlerstatus]", "ff25b509030d2800", "0320ff00", "mD"},
    {"r,message circuit,message name,message comment,,25,B509,0d2800,,,D2C,,°C,Temperatur,,,sensor",
        "\n    \"0\": {\"name\": \"\", \"value\": -14.00},\n    \"1\": {\"name\": \"sensor\", \"value\": \"ok\"}",
        "ff25b509030d2800", "0320ff00", "mj"},
    {"r,,name,,,25,B509,0d2800,,,tempsensorc", "-14.00", "ff25b509030d2800", "0320ff55", "m"},
    {"r,,name,,,25,B509,0d28,,m,sensorc,,,,,,temp", "-14.00", "ff25b509030d2855", "0220ff", "m"},
    {"u,,first,,,fe,0700,,x,,bda", "26.10.2014", "fffe07000426100614", "00", "p"},
    {"u,broadcast,hwStatus,,,fe,b505,27,,,UCH,,,,,,UCH,,,,,,UCH,,,", "0;19;0", "10feb505042700130097", "00", ""},
    {"w,,first,,,15,b509,0400,date,,bda", "26.10.2014", "ff15b50906040026100614", "00", "m"},
    {"w,,first,,,15,b509", "", "ff15b50900", "00", "m"},
    {"w,,,,,,b505,2d", "", "", "", "defaults"},
    {"w,,offset,,,50,,,,,temp", "0.50", "ff50b505042d080000", "00", "md"},
    {"r,ehp,time,,,08,b509,0d2800,,,time", "15:00:17", "ff08b509030d2800", "0311000f", "md"},
    {"r,ehp,time,,,08;10,b509,0d2800,,,time", "", "", "", "c"},
    {"r,ehp,time,,,08;09,b509,0d2800,,,time", "15:00:17", "ff08b509030d2800", "0311000f", "md*"},
    {"r,ehp,date,,,08,b509,0d2900,,,date", "23.11.2014", "ff08b509030d2900", "03170b0e", "md"},
    {"r,700,date,,,15,b524,020000003400,,,IGN:4,,,,,,date", "23.11.2015", "ff15b52406020000003400", "0703003400170b0f",
        "d"},
    {"r,700,time,,,15,b524,030000003500,,,IGN:4,,,,,,HTI", "12:29:06", "ff15b52406030000003500", "07030035000c1d06",
        "d"},
    {"", "23.11.2015", "ff15b52406020000003400", "0703003400170b0f", "d"},
    {"", "12:29:06", "ff15b52406030000003500", "07030035000c1d06", "d"},
    {"w,700,date,,,15,b524,020000003400,,,date", "23.11.2015", "ff15b52409020000003400170b0f", "00", "m"},
    {"r,ehp,error,,,08,b509,0d2800,index,m,UCH,,,,,,time", "3;15:00:17", "ff08b509040d280003", "0311000f", "mdi"},
    {"r,ehp,error,,,08,b509,0d2800,index,m,UCH,,,,,,time", "index=3;time=15:00:17", "ff08b509040d280003", "0311000f",
        "mD"},
    {"u,ehp,ActualEnvironmentPower,Energiebezug,,08,B509,29BA00,,s,IGN:2,,,,,s,power", "8", "1008b5090329ba00",
        "03ba0008", "pm"},
    {"uw,ehp,test,Test,,08,B5de,ab,,,power,,,,,s,hex:1", "8;39", "1008b5de02ab08", "0139", "pm"},
    {"u,ehp,hwTankTemp,Speichertemperatur IST,,25,B509,290000,,,IGN:2,,,,,,tempsensor", "", "", "", "M"},
    {"", "55.50;ok", "1025b50903290000", "050000780300", "d"},
    {"r,ehp,datetime,Datum Uhrzeit,,50,B504,00,,,dcfstate,,,,time,,BTI,,,,date,,BDA,,,,temp,,temp2",
        "valid;08:24:51;31.12.2014;-0.875", "1050b5040100", "0a035124083112031420ff", "md" },
    {"r,ehp,bad,invalid pos,,50,B5ff,000102,,m,HEX:8;tempsensor;tempsensor;tempsensor;tempsensor;power;power,,,", "",
        "", "", "c" },
    {"r,ehp,bad,invalid pos,,50,B5ff,,,s,HEX:8;tempsensor;tempsensor;tempsensor;tempsensor;tempsensor;power;power,,,",
        "", "", "", "c" },
    {"r,ehp,ApplianceCode,,,08,b509,0d4301,,,UCH,", "9", "ff08b509030d4301", "0109", "d" },
    {"r,ehp,,,,08,b509,0d", "", "", "", "defaults" },
    {"w,ehp,,,,08,b509,0e", "", "", "", "defaults" },
    {"[brinetowater],ehp,ApplianceCode,,,,4;6;8;9;10", "", "", "", "condition" },
    {"[airtowater]r,ehp,notavailable,,,,,0100,,,uch", "1", "", "", "c" },
    {"[brinetowater]r,ehp,available,,,,,0100,,,uch", "1", "ff08b509030d0100", "0101", "d" },
    {"r,,x,,,,,\"6800\",,,UCH,,,bit0=\"comment, continued comment", "", "", "", "c" },
    {"r,,x,,,,,\"6800\",,,UCH,,\"\",\"bit0=\"comment, continued comment\"", "=1 [bit0=\"comment, continued comment]",
        "ff08b509030d6800", "0101", "mD" },
    {"r,ehp,multi,,,,,0001:5;0002;0003,longname,,STR:15", "ABCDEFGHIJKLMNO",
        "ff08b509030d0001;ff08b509030d0003;ff08b509030d0002", "054142434445;054b4c4d4e4f;05464748494a", "mdC" },
    {"r,ehp,multi,,,,,01;02;03,longname,,STR:15", "ABCDEFGHIJKLMNO", "ff08b509020d01;ff08b509020d03;ff08b509020d02",
        "084142434445464748;054b4c4d4e4f;02494a", "mdC" },
    {"w,ehp,multi,,,,,01:8;02:2;03,longname,,STR:15", "ABCDEFGHIJKLMNO",
        "ff08b5090a0e014142434445464748;ff08b509040e02494a;ff08b509070e034b4c4d4e4f", "00;00;00", "mdC" },
    {"w,ehp,multi,,,,,01:8;02:2;0304,longname,,STR:15", "ABCDEFGHIJKLMNO",
        "ff08b5090a0e014142434445464748;ff08b509040e02494a;ff08b509070e034b4c4d4e4f", "00;00;00", "cC" },
    {"r,ehp,scan,chained scan,,08,B509,24:9;25;26;27,,,IGN,,,,id4,,STR:28", "21074500100027790000000000N8",
        "ff08b5090124;ff08b5090125;ff08b5090126;ff08b5090127",
        "09003231303734353030;09313030303237373930;09303030303030303030;024E38", "mdC" },
    {"r,,x,,,,,6900,,,UCH,10,bar,,Bit7,,BI7:1,0=B70;1=B71,,,Bit6,,BI6:1,0=B60;1=B61", "1.9;B71;B61",
        "ff08b509030d6900", "03138040", "md" },
    {"r,,x,,,,,6900,,,UCH,10,bar,,Bit7,,BI7:1,0=B70;1=B71,,,Bit6,,BI6:1,0=B60;1=B61", "1.9;B71;B60",
        "ff08b509030d6900", "0313ffbf", "md" },
    {"r,,x,,,,,6900,,,UCH,10,bar,,Bit7,,BI7:1,0=B70;1=B71,,,Bit6,,BI6:1,0=B60;1=B61", "1.9;B70;B61",
        "ff08b509030d6900", "03137fff", "md" },
    {"r,,x,,,,,6900,,,UCH,10,bar,,Bit7,,BI7:1,0=B70;1=B71,,,Bit6,,BI6:1,0=B60;1=B61", "1.9;B70;B60",
        "ff08b509030d6900", "03137fbf", "md" },
    {"r,,x,,,,,6a00,,,UCH,10,bar,,Bit6,,BI6:1,0=B60;1=B61,,,Bit7,,BI7:1,0=B70;1=B71", "1.9;B61;B71",
        "ff08b509030d6900", "0213ff", "md" },
    {"r,,x,,,,,6a00,,,UCH,10,bar,,Bit6,,BI6:1,0=B60;1=B61,,,Bit7,,BI7:1,0=B70;1=B71", "1.9;B60;B71",
        "ff08b509030d6900", "0213bf", "md" },
    {"r,,x,,,,,6a00,,,UCH,10,bar,,Bit6,,BI6:1,0=B60;1=B61,,,Bit7,,BI7:1,0=B70;1=B71", "1.9;B61;B70",
        "ff08b509030d6900", "02137f", "md" },
    {"r,,x,,,,,6a00,,,UCH,10,bar,,Bit6,,BI6:1,0=B60;1=B61,,,Bit7,,BI7:1,0=B70;1=B71", "1.9;B60;B70",
        "ff08b509030d6900", "02133f", "md" },
    {"r,cir*cuit#level,na*me,com*ment,ff,75,b509,0d", "", "", "", "defaults" },
    {"r,CIRCUIT,NAME,COMMENT,,,,0100,field,,UCH",
        "r,cirCIRCUITcuit,naNAMEme,comCOMMENTment,ff,75,b509,0d0100,field,s,UCH,,,: field=42",
        "ff08b509030d0100", "012a", "mDN"},
  };
  templates = new DataFieldTemplates();
  MessageMap* messages = new MessageMap();
  vector< vector<string> > defaultsRows;
  map<string, Condition*> &conditions = messages->getConditions();
  Message* message = NULL;
  vector<Message*> deleteMessages;
  vector<SymbolString*> mstrs;
  vector<SymbolString*> sstrs;
  mstrs.resize(1);
  sstrs.resize(1);
  for (size_t i = 0; i < sizeof(checks) / sizeof(checks[0]); i++) {
    string check[5] = checks[i];
    string inputStr = check[1];
    string flags = check[4];
    bool isTemplate = flags == "template";
    bool isCondition = flags == "condition";
    bool isDefaults = isCondition || flags == "defaults";
    bool dontMap = flags.find('m') != string::npos;
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
    vector<string> entries;
    istringstream ifs(check[0]);
    unsigned int lineNo = 0;
    if (!FileReader::splitFields(ifs, entries, lineNo)) {
      entries.clear();
    }
    if (isTemplate) {
      // store new template
      DataField* fields = NULL;
      vector<string>::iterator it = entries.begin();
      result = DataField::create(it, entries.end(), templates, fields, false, true, false);
      if (result != RESULT_OK) {
        cout << "\"" << check[0] << "\": template fields create error: " << getResultCode(result) << endl;
      } else if (it != entries.end()) {
        cout << "\"" << check[0] << "\": template fields create error: trailing input "
            << static_cast<unsigned>(entries.end()-it) << endl;
      } else {
        cout << "\"" << check[0] << "\": create template OK" << endl;
        result = templates->add(fields, "", true);
        if (result == RESULT_OK) {
          cout << "  store template OK" << endl;
        } else {
          cout << "  store template error: " << getResultCode(result) << endl;
          delete fields;
        }
      }
      continue;
    }
    if (isDefaults) {
      // store defaults or condition
      vector<string>::iterator it = entries.begin();
      size_t oldSize = conditions.size();
      result = messages->addDefaultFromFile(defaultsRows, entries, it, "", "", "", "no file", 1);
      if (result != RESULT_OK) {
        cout << "\"" << check[0] << "\": defaults read error: " << getResultCode(result) << endl;
      } else if (it != entries.end()) {
        cout << "\"" << check[0] << "\": defaults read error: trailing input "
            << static_cast<unsigned>(entries.end()-it) << endl;
      } else {
        cout << "\"" << check[0] << "\": read defaults OK" << endl;
        if (isCondition) {
          if (conditions.size() == oldSize) {
            cout << "  create condition error" << endl;
          } else {
            result = messages->resolveConditions();
            if (result != RESULT_OK) {
              cout << "  resolve conditions error: " << getResultCode(result) << " " << messages->getLastError()
                  << endl;
            } else {
              cout << "  resolve conditions OK" << endl;
            }
          }
        }
      }
      continue;
    }
    if (isChain) {
      size_t pos = 0;
      string token;
      istringstream stream(check[2]);
      while (getline(stream, token, VALUE_SEPARATOR)) {
        if (pos >= mstrs.size()) {
          mstrs.resize(pos+1);
        } else if (mstrs[pos] != NULL) {
          delete mstrs[pos];
        }
        mstrs[pos] = new SymbolString(false);
        result = mstrs[pos]->parseHex(token);
        if (result != RESULT_OK) {
          cout << "\"" << check[0] << "\": parse \"" << token << "\" error: " << getResultCode(result) << endl;
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
        } else if (sstrs[pos] != NULL) {
          delete sstrs[pos];
        }
        sstrs[pos] = new SymbolString(false);
        result = sstrs[pos]->parseHex(token);
        if (result != RESULT_OK) {
          cout << "\"" << check[0] << "\": parse \"" << token << "\" error: " << getResultCode(result) << endl;
          break;
        }
        pos++;
      }
      if (result != RESULT_OK) {
        continue;
      }
    } else {
      if (mstrs[0] != NULL) {
        delete mstrs[0];
      }
      mstrs[0] = new SymbolString(false);
      result = mstrs[0]->parseHex(check[2]);
      if (result != RESULT_OK) {
        cout << "\"" << check[0] << "\": parse \"" << check[2] << "\" error: " << getResultCode(result) << endl;
        continue;
      }
      if (sstrs[0] != NULL) {
        delete sstrs[0];
      }
      sstrs[0] = new SymbolString(false);
      result = sstrs[0]->parseHex(check[3]);
      if (result != RESULT_OK) {
        cout << "\"" << check[0] << "\": parse \"" << check[3] << "\" error: " << getResultCode(result) << endl;
        continue;
      }
    }

    if (deleteMessages.size() > 0) {
      for (vector<Message*>::iterator it = deleteMessages.begin(); it != deleteMessages.end(); it++) {
        Message* deleteMessage = *it;
        delete deleteMessage;
      }
      deleteMessages.clear();
    }
    if (entries.size() == 0) {
      message = messages->find(*mstrs[0]);
      if (message == NULL) {
        cout << "\"" << check[2] << "\": find error: NULL" << endl;
        continue;
      }
      cout << "\"" << check[2] << "\": find OK" << endl;
    } else {
      vector<string>::iterator it = entries.begin();
      string types = *it;
      Condition* condition = NULL;
      result = messages->readConditions(types, "no file", condition);
      if (result == RESULT_OK) {
        *it = types;
        result = Message::create(it, entries.end(), &defaultsRows, condition, "no file", templates, deleteMessages);
      }
      if (failedCreate) {
        if (result == RESULT_OK) {
          cout << "\"" << check[0] << "\": failed create error: unexpectedly succeeded" << endl;
        } else {
          cout << "\"" << check[0] << "\": failed create OK" << endl;
        }
        continue;
      }
      if (result != RESULT_OK) {
        cout << "\"" << check[0] << "\": create error: "
            << getResultCode(result) << endl;
        printErrorPos(cout, entries.begin(), entries.end(), it, "", 0, result);
        continue;
      }
      if (deleteMessages.size() == 0) {
        cout << "\"" << check[0] << "\": create error: NULL" << endl;
        continue;
      }
      if (it != entries.end()) {
        cout << "\"" << check[0] << "\": create error: trailing input " << static_cast<unsigned>(entries.end()-it)
            << endl;
        continue;
      }
      if (multi && deleteMessages.size() == 1) {
        cout << "\"" << check[0] << "\": create error: single message instead of multiple" << endl;
        continue;
      }
      if (!multi && deleteMessages.size() > 1) {
        cout << "\"" << check[0] << "\": create error: multiple messages instead of single" << endl;
        continue;
      }
      cout << "\"" << check[0] << "\": create OK" << endl;
      if (!dontMap) {
        result_t result = RESULT_OK;
        for (vector<Message*>::iterator it = deleteMessages.begin(); it != deleteMessages.end(); it++) {
          Message* deleteMessage = *it;
          result_t result = messages->add(deleteMessage);
          if (result != RESULT_OK) {
            cout << "\"" << check[0] << "\": add error: "
                << getResultCode(result) << endl;
            break;
          }
        }
        if (result != RESULT_OK) {
          continue;
        }
        cout << "  map OK" << endl;
        message = deleteMessages.front();
        deleteMessages.clear();
        if (onlyMap) {
          continue;
        }
        Message* foundMessage = messages->find(*mstrs[0]);
        if (foundMessage == message) {
          cout << "  find OK" << endl;
        } else if (foundMessage == NULL) {
          cout << "  find error: NULL" << endl;
        } else {
          cout << "  find error: different" << endl;
        }
      } else {
        message = deleteMessages.front();
      }
    }

    if (message->isPassive() || decode) {
      ostringstream output;
      for (unsigned char index = 0; index < message->getCount(); index++) {
        message->storeLastData(*mstrs[index], *sstrs[index]);
      }
      if (withMessageDump && !decodeJson) {
        message->dump(output, NULL, true);
        output << ": ";
      }
      result = message->decodeLastData(output,
          (decodeVerbose?OF_NAMES|OF_UNITS|OF_COMMENTS:0)|(decodeJson?OF_NAMES|OF_JSON:0), false);
      if (result != RESULT_OK) {
        cout << "  \"" << check[2] << "\" / \"" << check[3] << "\": decode error: "
            << getResultCode(result) << endl;
        continue;
      }
      cout << "  \"" << check[2] << "\" / \"" << check[3] <<  "\": decode OK" << endl;
      bool match = inputStr == output.str();
      verify(false, "decode", check[2] + "/" + check[3], match, inputStr, output.str());
    }
    if (!message->isPassive() && (withInput || !decode)) {
      istringstream input(inputStr);
      SymbolString writeMstr(false);
      result = message->prepareMaster(0xff, writeMstr, input);
      if (failedPrepare) {
        if (result == RESULT_OK) {
          cout << "  \"" << inputStr << "\": failed prepare error: unexpectedly succeeded" << endl;
        } else {
          cout << "  \"" << inputStr << "\": failed prepare OK" << endl;
        }
        continue;
      }

      if (result != RESULT_OK) {
        cout << "  \"" << inputStr << "\": prepare error: "
            << getResultCode(result) << endl;
        continue;
      }
      cout << "  \"" << inputStr << "\": prepare OK" << endl;

      bool match = writeMstr == *mstrs[0];
      verify(failedPrepareMatch, "prepare", inputStr, match, mstrs[0]->getDataStr(true, false),
          writeMstr.getDataStr(true, false));
    }
  }

  if (deleteMessages.size() > 0) {
    for (vector<Message*>::iterator it = deleteMessages.begin(); it != deleteMessages.end(); it++) {
      Message* deleteMessage = *it;
      delete deleteMessage;
    }
    deleteMessages.clear();
  }

  delete templates;
  delete messages;
  for (vector<SymbolString*>::iterator it = mstrs.begin(); it != mstrs.end(); it++) {
    delete *it;
  }
  for (vector<SymbolString*>::iterator it = sstrs.begin(); it != sstrs.end(); it++) {
    delete *it;
  }
  return 0;
}
