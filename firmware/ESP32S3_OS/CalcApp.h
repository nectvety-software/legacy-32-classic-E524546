
/*
  Calculator App for ESP32-S3 Nokia OS
  Máy tính khoa học đơn giản
*/

#ifndef CALC_APP_H
#define CALC_APP_H

#include <Arduino.h>
#include <cmath>

class Calculator {
private:
  String display;
  String equation;
  double memory;
  bool clearOnNext;

  double evaluate(const String& expr) {
    // Simple expression evaluator
    // Hỗ trợ: +, -, *, /, ^, sin, cos, tan, log, sqrt, pi, e

    String e = expr;
    e.replace(" ", "");
    e.replace("PI", String(PI));
    e.replace("pi", String(PI));
    e.toLowerCase();

    // Xử lý ngoặc đơn giản
    int openParen = e.indexOf('(');
    while (openParen >= 0) {
      int closeParen = e.indexOf(')', openParen);
      if (closeParen < 0) break;

      String inside = e.substring(openParen + 1, closeParen);
      double val = evaluate(inside);

      // Xử lý hàm trước ngoặc
      int funcStart = openParen - 1;
      while (funcStart >= 0 && isAlpha(e[funcStart])) funcStart--;
      funcStart++;

      String func = e.substring(funcStart, openParen);
      double result = val;

      if (func == "sin") result = sin(val);
      else if (func == "cos") result = cos(val);
      else if (func == "tan") result = tan(val);
      else if (func == "log") result = log(val);
      else if (func == "sqrt") result = sqrt(val);
      else if (func == "abs") result = abs(val);

      e = e.substring(0, funcStart) + String(result, 6) + e.substring(closeParen + 1);
      openParen = e.indexOf('(');
    }

    // Xử lý toán tử theo thứ tự ưu tiên
    return parseExpression(e);
  }

  double parseExpression(String& e) {
    // Tách theo + và -
    std::vector<double> terms;
    std::vector<char> ops;

    int start = 0;
    for (int i = 0; i < e.length(); i++) {
      if (e[i] == '+' || e[i] == '-') {
        if (i > 0 && (e[i-1] == 'e' || e[i-1] == 'E')) continue; // Scientific notation

        terms.push_back(parseTerm(e.substring(start, i)));
        ops.push_back(e[i]);
        start = i + 1;
      }
    }
    terms.push_back(parseTerm(e.substring(start)));

    double result = terms[0];
    for (int i = 0; i < ops.size(); i++) {
      if (ops[i] == '+') result += terms[i+1];
      else result -= terms[i+1];
    }

    return result;
  }

  double parseTerm(String e) {
    // Tách theo * và /
    std::vector<double> factors;
    std::vector<char> ops;

    int start = 0;
    for (int i = 0; i < e.length(); i++) {
      if (e[i] == '*' || e[i] == '/') {
        factors.push_back(parseFactor(e.substring(start, i)));
        ops.push_back(e[i]);
        start = i + 1;
      }
    }
    factors.push_back(parseFactor(e.substring(start)));

    double result = factors[0];
    for (int i = 0; i < ops.size(); i++) {
      if (ops[i] == '*') result *= factors[i+1];
      else result /= factors[i+1];
    }

    return result;
  }

  double parseFactor(String e) {
    e.trim();
    if (e.length() == 0) return 0;

    // Xử lý lũy thừa
    int caret = e.indexOf('^');
    if (caret > 0) {
      double base = parseFactor(e.substring(0, caret));
      double exp = parseFactor(e.substring(caret + 1));
      return pow(base, exp);
    }

    return e.toDouble();
  }

public:
  Calculator() : memory(0), clearOnNext(false) {
    clear();
  }

  void clear() {
    display = "0";
    equation = "";
    clearOnNext = false;
  }

  void input(char c) {
    if (clearOnNext) {
      display = "";
      clearOnNext = false;
    }

    if (c >= '0' && c <= '9') {
      if (display == "0") display = "";
      display += c;
    } else if (c == '.') {
      if (display.indexOf('.') < 0) display += c;
    } else if (c == '+' || c == '-' || c == '*' || c == '/' || c == '^') {
      equation = display + " " + c + " ";
      clearOnNext = true;
    } else if (c == '=') {
      equation += display;
      double result = evaluate(equation);
      display = String(result, 6);
      // Trim trailing zeros
      while (display.endsWith("0") && display.indexOf('.') > 0) {
        display.remove(display.length() - 1);
      }
      if (display.endsWith(".")) display.remove(display.length() - 1);
      equation = "";
      clearOnNext = true;
    } else if (c == 'C') {
      clear();
    } else if (c == 'M') {
      memory = display.toDouble();
    } else if (c == 'R') {
      display = String(memory, 6);
    } else if (c == 'D') {
      if (display.length() > 1) {
        display.remove(display.length() - 1);
      } else {
        display = "0";
      }
    }
  }

  String getDisplay() { return display; }
  String getEquation() { return equation; }

  void draw(TFT_eSPI& tft, int x, int y, int w, int h) {
    // Draw calculator interface
    tft.fillRect(x, y, w, h, 0x2104);
    tft.drawRect(x, y, w, h, 0xC618);

    // Display area
    tft.fillRect(x + 5, y + 5, w - 10, 40, 0x0000);
    tft.setTextColor(0x07E0, 0x0000);
    tft.setTextSize(2);
    tft.setCursor(x + 10, y + 15);

    // Scroll if too long
    String d = display;
    if (d.length() > 12) {
      d = d.substring(d.length() - 12);
    }
    tft.print(d);

    // Buttons
    const char* buttons[] = {
      "7", "8", "9", "/",
      "4", "5", "6", "*",
      "1", "2", "3", "-",
      "0", ".", "=", "+",
      "C", "M", "R", "D"
    };

    int btnW = (w - 25) / 4;
    int btnH = (h - 60) / 5;

    for (int i = 0; i < 20; i++) {
      int bx = x + 5 + (i % 4) * (btnW + 5);
      int by = y + 50 + (i / 4) * (btnH + 5);

      tft.fillRect(bx, by, btnW, btnH, 0x3A18);
      tft.setTextColor(0xFFFF, 0x3A18);
      tft.setTextSize(1);
      tft.setCursor(bx + btnW/2 - 3, by + btnH/2 - 4);
      tft.print(buttons[i]);
    }
  }
};

#endif
