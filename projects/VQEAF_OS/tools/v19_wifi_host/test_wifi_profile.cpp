#include "../../src/services/WiFiProfileStore.h"
#include <map>
#include <string>
#include <cassert>
#include <cstdio>
namespace FakeNvs {
std::map<std::string,String> values;
int writes=0;
}
int main(){
 WiFiProfileStore p;p.begin();
 assert(p.count()==0);
 assert(FakeNvs::writes==0);
 p.saveProfile("", "validpass", false); // invalid empty SSID
 p.saveProfile("Home","12345",false); // invalid short WPA password
 assert(p.count()==0&&FakeNvs::writes==0);
 p.saveProfile("Home","validpass",false);
 assert(p.count()==1&&p.lastSSID()=="Home");
 const int writes=FakeNvs::writes;
 for(int i=0;i<100;i++)p.saveProfile("Home","validpass",false);
 assert(FakeNvs::writes==writes); // no NVS wear after 100 unchanged reconnects
 p.saveProfile("Cafe","",true);
 assert(p.count()==2&&p.at(0).ssid=="Cafe"&&p.at(0).password.length()==0);
 // Malformed on-card credentials, duplicates, and count beyond bounded max.
 FakeNvs::values.clear();FakeNvs::writes=0;
 FakeNvs::values["symbian-net:count"]="6";
 FakeNvs::values["symbian-net:s0"]="OK";
 FakeNvs::values["symbian-net:p0"]="12345678";
 FakeNvs::values["symbian-net:o0"]="0";
 FakeNvs::values["symbian-net:s1"]="OK"; // duplicate
 FakeNvs::values["symbian-net:p1"]="different";
 FakeNvs::values["symbian-net:o1"]="0";
 FakeNvs::values["symbian-net:s2"]="Bad";
 FakeNvs::values["symbian-net:p2"]="short";
 FakeNvs::values["symbian-net:o2"]="0";
 FakeNvs::values["symbian-net:last"]="Bad";
 WiFiProfileStore repaired;repaired.begin();
 assert(repaired.count()==1&&repaired.lastSSID()=="OK"&&FakeNvs::writes>0);
 assert(FakeNvs::values["symbian-net:count"]=="1");
 FakeNvs::writes=0;
 WiFiProfileStore clean;clean.begin();
 assert(clean.count()==1&&FakeNvs::writes==0);
 puts("v1.9 real WiFi profile NVS validation and write coalescing: PASS");
}
