#ifndef CALC_APP_H
#define CALC_APP_H

#include "../core/AppManager.h"

class CalcApp : public App {
public:
    const char* getName() override { return "Calculator"; }
    void begin() override;
    void update() override;
    bool onKey(uint8_t key) override;

private:
    void draw();
    void appendDigit(int d);
    void setOperation(char op);
    void calculate();
    
    char display[16] = "0";
    float operand1 = 0, operand2 = 0;
    char operation = 0;
    bool startNewNumber = true;
    int selectedBtn = 0;
};

#endif
