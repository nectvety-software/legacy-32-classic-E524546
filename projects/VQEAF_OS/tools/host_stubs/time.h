#pragma once
#include_next <time.h>
#include <Arduino.h>
inline bool getLocalTime(struct tm* t,uint32_t=5000){ if(t) memset(t,0,sizeof(*t)); return false;} inline void configTime(long,int,const char*,const char*){}
