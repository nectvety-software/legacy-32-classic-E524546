#ifndef LUA_INTERPRETER_H
#define LUA_INTERPRETER_H

#include <Arduino.h>
#include <FS.h>
#include "component/Config.h"
#include "component/ButtonManager.h"

struct LuaVar {
    String name;
    String value;
    bool isNumber;
};

struct LuaFunction {
    String name;
    std::vector<String> params;
    std::vector<String> body;
};

class LuaInterpreter {
private:
    std::vector<std::vector<LuaVar>> scopes;
    std::vector<LuaFunction> functions;
    String output;
    bool hasError;
    String errorMsg;
    bool hasReturn;
    String returnValue;

    String trimString(String s) {
        while (s.length() && isWhitespace(s[0])) s.remove(0, 1);
        while (s.length() && isWhitespace(s[s.length() - 1])) s.remove(s.length() - 1, 1);
        return s;
    }

    bool isWhitespace(char c) {
        return c == ' ' || c == '\t' || c == '\r' || c == '\n';
    }

    bool isNumeric(const String &s) {
        String value = s;
        value.trim();
        if (value.length() == 0) return false;
        int start = 0;
        if (value[0] == '+' || value[0] == '-') start = 1;
        bool hasDot = false;
        for (int i = start; i < value.length(); i++) {
            char c = value[i];
            if (c == '.') {
                if (hasDot) return false;
                hasDot = true;
            } else if (!isdigit(c)) {
                return false;
            }
        }
        return start < value.length();
    }

    String stripQuotes(const String &s) {
        if (s.length() >= 2 && s.startsWith("\"") && s.endsWith("\"")) return s.substring(1, s.length() - 1);
        return s;
    }

    void pushScope() {
        scopes.emplace_back();
    }

    void popScope() {
        if (!scopes.empty()) scopes.pop_back();
    }

    String getVarValue(const String &varName) {
        for (int i = scopes.size() - 1; i >= 0; i--) {
            for (auto &v : scopes[i]) {
                if (v.name == varName) return v.value;
            }
        }
        return "";
    }

    void setVar(const String &name, const String &value) {
        String key = trimString(name);
        if (key.length() == 0) return;
        for (int i = scopes.size() - 1; i >= 0; i--) {
            for (auto &v : scopes[i]) {
                if (v.name == key) {
                    v.value = trimString(value);
                    v.isNumber = isNumeric(v.value);
                    return;
                }
            }
        }
        if (scopes.empty()) pushScope();
        LuaVar v;
        v.name = key;
        v.value = trimString(value);
        v.isNumber = isNumeric(v.value);
        scopes.back().push_back(v);
    }

    std::vector<String> splitArgs(const String &text) {
        std::vector<String> result;
        String current = "";
        bool inQuote = false;
        for (int i = 0; i < text.length(); i++) {
            char c = text[i];
            if (c == '"') {
                inQuote = !inQuote;
                current += c;
            } else if (c == ',' && !inQuote) {
                result.push_back(trimString(current));
                current = "";
            } else {
                current += c;
            }
        }
        if (current.length()) result.push_back(trimString(current));
        return result;
    }

    int findAssignmentOperator(const String &line) {
        bool inQuote = false;
        for (int i = 0; i < line.length(); i++) {
            char c = line[i];
            if (c == '"') {
                inQuote = !inQuote;
            }
            if (inQuote) continue;
            if (c == '=') {
                if (i > 0 && (line[i - 1] == '=' || line[i - 1] == '<' || line[i - 1] == '>' || line[i - 1] == '~')) continue;
                if (i + 1 < line.length() && line[i + 1] == '=') continue;
                return i;
            }
        }
        return -1;
    }

    LuaFunction *findFunction(const String &name) {
        for (auto &fn : functions) {
            if (fn.name == name) return &fn;
        }
        return nullptr;
    }

    int parseIntArg(const String &arg) {
        String value = evalExpr(arg);
        return value.toInt();
    }

    uint16_t parseColorArg(const String &arg) {
        int c = parseIntArg(arg);
        return (uint16_t)c;
    }

    bool isFunctionCall(const String &expr) {
        int paren = expr.indexOf('(');
        return paren > 0 && expr.endsWith(")");
    }

    String callBuiltin(const String &name, const std::vector<String> &args) {
        if (name == "cls" || name == "clear") {
            tft.fillScreen(TFT_BLACK);
            return "";
        }
        if (name == "sleep") {
            int ms = parseIntArg(args[0]);
            unsigned long t0 = millis();
            while (millis() - t0 < (unsigned long)ms) {
                buttonManager.update();
                delay(2);
            }
            return "";
        }
        if (name == "poll") {
            buttonManager.update();
            return "";
        }
        if (name == "millis") {
            return String(millis());
        }
        if (name == "random") {
            int a = parseIntArg(args[0]);
            int b = parseIntArg(args[1]);
            return String(random(a, b));
        }
        if (name == "btn") {
            return buttonManager.isPressed(parseIntArg(args[0])) ? String("true") : String("false");
        }
        if (name == "btnp") {
            return buttonManager.isJustPressed(parseIntArg(args[0])) ? String("true") : String("false");
        }
        if (name == "color") {
            int r = parseIntArg(args[0]);
            int g = parseIntArg(args[1]);
            int b = parseIntArg(args[2]);
            return String((int)tft.color565(r, g, b));
        }
        if (name == "line") {
            int x1 = parseIntArg(args[0]);
            int y1 = parseIntArg(args[1]);
            int x2 = parseIntArg(args[2]);
            int y2 = parseIntArg(args[3]);
            uint16_t c = parseColorArg(args[4]);
            tft.drawLine(x1, y1, x2, y2, c);
            return "";
        }
        if (name == "rect") {
            int x = parseIntArg(args[0]);
            int y = parseIntArg(args[1]);
            int w = parseIntArg(args[2]);
            int h = parseIntArg(args[3]);
            uint16_t c = parseColorArg(args[4]);
            tft.drawRect(x, y, w, h, c);
            return "";
        }
        if (name == "fillRect") {
            int x = parseIntArg(args[0]);
            int y = parseIntArg(args[1]);
            int w = parseIntArg(args[2]);
            int h = parseIntArg(args[3]);
            uint16_t c = parseColorArg(args[4]);
            tft.fillRect(x, y, w, h, c);
            return "";
        }
        if (name == "circle") {
            int x = parseIntArg(args[0]);
            int y = parseIntArg(args[1]);
            int r = parseIntArg(args[2]);
            uint16_t c = parseColorArg(args[3]);
            tft.drawCircle(x, y, r, c);
            return "";
        }
        if (name == "fillCircle") {
            int x = parseIntArg(args[0]);
            int y = parseIntArg(args[1]);
            int r = parseIntArg(args[2]);
            uint16_t c = parseColorArg(args[3]);
            tft.fillCircle(x, y, r, c);
            return "";
        }
        if (name == "text") {
            int x = parseIntArg(args[0]);
            int y = parseIntArg(args[1]);
            String txt = stripQuotes(args[2]);
            uint16_t c = parseColorArg(args[3]);
            int size = 1;
            if (args.size() > 4) size = parseIntArg(args[4]);
            tft.setTextColor(c, TFT_BLACK);
            tft.setTextSize(size);
            tft.setCursor(x, y);
            tft.print(txt);
            return "";
        }
        if (name == "sin") {
            float v = evalExpr(args[0]).toFloat();
            return String(sin(v));
        }
        if (name == "cos") {
            float v = evalExpr(args[0]).toFloat();
            return String(cos(v));
        }
        if (name == "abs") {
            float v = evalExpr(args[0]).toFloat();
            return String(abs(v));
        }
        if (name == "tonumber") {
            return String(parseIntArg(args[0]));
        }
        if (name == "pixel") {
            int x = parseIntArg(args[0]);
            int y = parseIntArg(args[1]);
            uint16_t c = parseColorArg(args[2]);
            tft.drawPixel(x, y, c);
            return "";
        }
        if (name == "upper") {
            String s = stripQuotes(args[0]);
            s.toUpperCase();
            return s;
        }
        if (name == "len") {
            return String(stripQuotes(args[0]).length());
        }
        if (name == "sub") {
            String s = stripQuotes(args[0]);
            int a = parseIntArg(args[1]);
            int b = parseIntArg(args[2]);
            if (a < 0) a = 0;
            if (b > (int)s.length()) b = s.length();
            if (a >= b) return String("");
            return s.substring(a, b);
        }
        if (name == "str") {
            return String(parseIntArg(args[0]));
        }
        if (name == "num") {
            return String(evalExpr(args[0]).toFloat());
        }
        return "";
    }

    String callFunction(const String &name, const std::vector<String> &args) {
        String builtin = callBuiltin(name, args);
        if (builtin.length() || name == "cls" || name == "clear" || name == "sleep" || name == "poll" || name == "millis" || name == "random" || name == "btn" || name == "btnp" || name == "color" || name == "line" || name == "rect" || name == "fillRect" || name == "circle" || name == "fillCircle" || name == "text" || name == "pixel" || name == "upper" || name == "len" || name == "sub" || name == "str" || name == "num" || name == "sin" || name == "cos" || name == "abs" || name == "tonumber") {
            return builtin;
        }

        LuaFunction *fn = findFunction(name);
        if (!fn) return "";
        pushScope();
        for (int i = 0; i < fn->params.size(); i++) {
            String param = trimString(fn->params[i]);
            String argValue = (i < args.size()) ? evalExpr(args[i]) : "";
            setVar(param, argValue);
        }
        hasReturn = false;
        returnValue = "";
        executeLines(fn->body, 0, fn->body.size());
        String result = returnValue;
        popScope();
        return result;
    }

    String evalExpr(String expr) {
        expr = trimString(expr);
        if (expr.length() == 0) return "";
        if (expr == "true") return "true";
        if (expr == "false") return "false";
        if (expr.startsWith("\"") && expr.endsWith("\"")) return stripQuotes(expr);

        if (expr.startsWith("not ")) {
            return evalBoolExpr(expr.substring(4)) ? String("false") : String("true");
        }

        int concatPos = expr.indexOf("..");
        if (concatPos >= 0) {
            String left = evalExpr(expr.substring(0, concatPos));
            String right = evalExpr(expr.substring(concatPos + 2));
            return left + right;
        }

        const char *comparators[] = {">=", "<=", "==", "~=", ">", "<"};
        for (int idx = 0; idx < 6; idx++) {
            String op = String(comparators[idx]);
            int pos = expr.indexOf(op);
            if (pos > 0) {
                String left = trimString(expr.substring(0, pos));
                String right = trimString(expr.substring(pos + op.length()));
                String aval = evalExpr(left);
                String bval = evalExpr(right);
                bool result = false;
                bool leftNum = isNumeric(aval);
                bool rightNum = isNumeric(bval);
                if (op == "==") {
                    result = aval == bval;
                } else if (op == "~=") {
                    result = aval != bval;
                } else if (leftNum && rightNum) {
                    float a = aval.toFloat();
                    float b = bval.toFloat();
                    if (op == ">=") result = a >= b;
                    else if (op == "<=") result = a <= b;
                    else if (op == ">") result = a > b;
                    else if (op == "<") result = a < b;
                } else {
                    if (op == ">=") result = aval >= bval;
                    else if (op == "<=") result = aval <= bval;
                    else if (op == ">") result = aval > bval;
                    else if (op == "<") result = aval < bval;
                }
                return result ? String("true") : String("false");
            }
        }

        int andPos = expr.indexOf(" and ");
        if (andPos > 0) {
            return (evalBoolExpr(expr.substring(0, andPos)) && evalBoolExpr(expr.substring(andPos + 5))) ? String("true") : String("false");
        }
        int orPos = expr.indexOf(" or ");
        if (orPos > 0) {
            return (evalBoolExpr(expr.substring(0, orPos)) || evalBoolExpr(expr.substring(orPos + 4))) ? String("true") : String("false");
        }

        if (isFunctionCall(expr)) {
            int paren = expr.indexOf('(');
            String name = trimString(expr.substring(0, paren));
            String argsText = expr.substring(paren + 1, expr.length() - 1);
            std::vector<String> args = splitArgs(argsText);
            return callFunction(name, args);
        }

        if (expr.indexOf('+') > 0) {
            int pos = expr.indexOf('+');
            String left = trimString(expr.substring(0, pos));
            String right = trimString(expr.substring(pos + 1));
            String lv = evalExpr(left);
            String rv = evalExpr(right);
            if (isNumeric(lv) && isNumeric(rv)) {
                float av = lv.toFloat();
                float bv = rv.toFloat();
                return String(av + bv);
            }
            return lv + rv;
        }
        if (expr.indexOf('*') > 0) {
            int pos = expr.indexOf('*');
            float av = evalExpr(trimString(expr.substring(0, pos))).toFloat();
            float bv = evalExpr(trimString(expr.substring(pos + 1))).toFloat();
            return String(av * bv);
        }
        if (expr.indexOf('/') > 0) {
            int pos = expr.indexOf('/');
            float av = evalExpr(trimString(expr.substring(0, pos))).toFloat();
            float bv = evalExpr(trimString(expr.substring(pos + 1))).toFloat();
            return String(av / bv);
        }
        int minusPos = expr.indexOf('-', 1);
        if (minusPos > 0) {
            float av = evalExpr(trimString(expr.substring(0, minusPos))).toFloat();
            float bv = evalExpr(trimString(expr.substring(minusPos + 1))).toFloat();
            return String(av - bv);
        }

        String value = getVarValue(expr);
        return value.length() ? value : expr;
    }

    bool evalBoolExpr(const String &expr) {
        String result = evalExpr(expr);
        result.trim();
        if (result == "false" || result == "0" || result == "" || result == "nil") return false;
        return true;
    }

    int findMatchingEnd(const std::vector<String> &lines, int start, const String &keyword) {
        int depth = 0;
        for (int i = start + 1; i < lines.size(); i++) {
            String cur = trimString(lines[i]);
            if (cur.startsWith(keyword + " ")) {
                depth++;
            }
            if (cur == "end") {
                if (depth == 0) return i;
                depth--;
            }
        }
        return lines.size() - 1;
    }

    int findElseIndex(const std::vector<String> &lines, int start, int end) {
        for (int i = start; i < end; i++) {
            String cur = trimString(lines[i]);
            if (cur == "else") return i;
        }
        return -1;
    }

    int parseFunctionDefinition(const std::vector<String> &lines, int i) {
        String line = trimString(lines[i]);
        int paren = line.indexOf('(');
        if (paren < 0) return i + 1;
        String name = trimString(line.substring(8, paren));
        String paramsText = line.substring(paren + 1, line.lastIndexOf(')'));
        std::vector<String> params = splitArgs(paramsText);
        LuaFunction fn;
        fn.name = name;
        fn.params = params;
        int end = findMatchingEnd(lines, i, "function");
        for (int j = i + 1; j < end; j++) {
            fn.body.push_back(lines[j]);
        }
        functions.push_back(fn);
        return end + 1;
    }

    int executeLines(const std::vector<String> &lines, int start, int end) {
        int i = start;
        while (i < end) {
            i = executeLine(lines, i);
            if (hasReturn) break;
        }
        return i;
    }

    int executeLine(const std::vector<String> &lines, int i) {
        String line = trimString(lines[i]);
        if (line.length() == 0 || line.startsWith("--")) return i + 1;

        if (line.startsWith("function ")) {
            return parseFunctionDefinition(lines, i);
        }

        if (line.startsWith("return ")) {
            returnValue = evalExpr(line.substring(7));
            hasReturn = true;
            return i + 1;
        }

        if (line.startsWith("if ") && line.endsWith(" then")) {
            String cond = trimString(line.substring(3, line.length() - 5));
            int end = findMatchingEnd(lines, i, "if");
            int elseIndex = findElseIndex(lines, i + 1, end);
            if (evalBoolExpr(cond)) {
                int blockEnd = (elseIndex >= 0) ? elseIndex : end;
                executeLines(lines, i + 1, blockEnd);
            } else if (elseIndex >= 0) {
                executeLines(lines, elseIndex + 1, end);
            }
            return end + 1;
        }

        if (line.startsWith("while ") && line.endsWith(" do")) {
            String cond = trimString(line.substring(6, line.length() - 3));
            int end = findMatchingEnd(lines, i, "while");
            while (evalBoolExpr(cond)) {
                executeLines(lines, i + 1, end);
                if (hasReturn) break;
            }
            return end + 1;
        }

        if (line.startsWith("for ") && line.endsWith(" do")) {
            String header = trimString(line.substring(4, line.length() - 3));
            int eq = header.indexOf('=');
            if (eq > 0) {
                String var = trimString(header.substring(0, eq));
                String range = trimString(header.substring(eq + 1));
                std::vector<String> parts = splitArgs(range);
                float startV = parts.size() > 0 ? evalExpr(parts[0]).toFloat() : 0;
                float stopV = parts.size() > 1 ? evalExpr(parts[1]).toFloat() : 0;
                float stepV = parts.size() > 2 ? evalExpr(parts[2]).toFloat()
                                               : (startV <= stopV ? 1 : -1);
                int end = findMatchingEnd(lines, i, "for");
                if (stepV == 0) stepV = 1;
                for (float v = startV;
                     (stepV > 0 ? v <= stopV : v >= stopV); v += stepV) {
                    setVar(var, String(v));
                    executeLines(lines, i + 1, end);
                    if (hasReturn) break;
                }
                return end + 1;
            }
        }

        if (line.startsWith("print(") && line.endsWith(")")) {
            String content = line.substring(6, line.length() - 1);
            content.trim();
            if (content.startsWith("\"") && content.endsWith("\"")) output += content.substring(1, content.length() - 1) + "\n";
            else output += evalExpr(content) + "\n";
            return i + 1;
        }

        if (line.startsWith("sleep(") && line.endsWith(")")) {
            String ms = trimString(line.substring(6, line.length() - 1));
            unsigned long t0 = millis();
            unsigned long target = (unsigned long)ms.toInt();
            while (millis() - t0 < target) {
                buttonManager.update();
                delay(2);
            }
            return i + 1;
        }

        int assignPos = findAssignmentOperator(line);
        if (assignPos >= 0) {
            String varName = trimString(line.substring(0, assignPos));
            String varValue = trimString(line.substring(assignPos + 1));
            setVar(varName, evalExpr(varValue));
            return i + 1;
        }

        if (isFunctionCall(line)) {
            evalExpr(line);
            return i + 1;
        }

        return i + 1;
    }

public:
    String runScript(String scriptPath) {
        output = "";
        hasError = false;
        errorMsg = "";
        hasReturn = false;
        returnValue = "";
        scopes.clear();
        functions.clear();
        pushScope();

        File f = SD.open(scriptPath);
        if (!f) { hasError = true; return "Error: Cannot open file"; }

        String fullScript = "";
        while (f.available()) fullScript += (char)f.read();
        f.close();

        std::vector<String> lines;
        int lineStart = 0;
        for (int i = 0; i <= fullScript.length(); i++) {
            if (i == fullScript.length() || fullScript[i] == '\n') {
                String line = fullScript.substring(lineStart, i);
                line.trim();
                lines.push_back(line);
                lineStart = i + 1;
            }
        }

        executeLines(lines, 0, lines.size());
        return output;
    }

    void clear() { output = ""; scopes.clear(); functions.clear(); hasError = false; hasReturn = false; }
};

static LuaInterpreter lua;

inline void initLua() {
    Serial.println("Lua Interpreter initialized");
}

#endif
