#include "CalculatorEngine.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
CalculatorEngine::CalculatorEngine(): accumulator(0), operation(0), nextNumber(false), fault(false) {
  strcpy(input,"0"); refresh();
}
void CalculatorEngine::refresh(){
  snprintf(screen,sizeof screen,"%s", fault ? "Math error" : input);
}
void CalculatorEngine::calculate(double rhs) {
  switch(operation) {
    case '+': accumulator += rhs; break;
    case '-': accumulator -= rhs; break;
    case '*': accumulator *= rhs; break;
    case '/': if(rhs==0.0){fault=true;return;} accumulator /= rhs; break;
    default: accumulator=rhs; break;
  }
  if (!isfinite(accumulator) || fabs(accumulator)>9.0e14) { fault=true; return; }
  snprintf(input,sizeof input,"%.10g",accumulator);
}
void CalculatorEngine::press(char key){
  if(key=='C'){ accumulator=0; operation=0; fault=false; nextNumber=false; strcpy(input,"0"); refresh(); return; }
  if(fault) return;
  if((key>='0'&&key<='9')||key=='.') {
    if(nextNumber){strcpy(input,"0");nextNumber=false;}
    size_t n=strlen(input);
    if(key=='.'){
      if(strchr(input,'.') || strchr(input,'e')) return;
      if(n+1<sizeof input){input[n]='.';input[n+1]=0;}
    }else if(n==1&&input[0]=='0') { input[0]=key; }
    else if(n+1<sizeof input) { input[n]=key; input[n+1]=0; }
  }else if(key=='<'){
    if(nextNumber){nextNumber=false;strcpy(input,"0");}
    else{size_t n=strlen(input);if(n>1)input[n-1]=0;else strcpy(input,"0");}
  }else if(key=='+'||key=='-'||key=='*'||key=='/') {
    if(!nextNumber) {
      if(operation) calculate(strtod(input,nullptr));
      else accumulator=strtod(input,nullptr);
      if(fault){refresh();return;}
    }
    operation=key; nextNumber=true;
  }else if(key=='='){
    if(operation&&!nextNumber) calculate(strtod(input,nullptr));
    operation=0; nextNumber=true;
  }
  refresh();
}
