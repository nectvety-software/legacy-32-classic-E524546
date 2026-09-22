#pragma once
#include <Arduino.h>
#include "UITheme.h"

// ═══════════════════════════════════════════════════════════
//  Snake Game - Classic snake for LegacyOS
// ═══════════════════════════════════════════════════════════

namespace AppSnake {
  #define GRID_W   20
  #define GRID_H   20
  #define CELL_SZ  12
  #define GRID_OX  20
  #define GRID_OY  20

  struct Point { int x, y; };
  
  static std::vector<Point> snake;
  static Point  food;
  static int    dir = 0;  // 0=R 1=D 2=L 3=U
  static int    nextDir = 0;
  static bool   alive = true;
  static int    score = 0;
  static int    highScore = 0;
  static uint32_t lastMove = 0;
  static int    speed = 200;  // ms per step
  static bool   started = false;
  static int    level = 1;
  
  void _placeFood() {
    bool ok = false;
    while (!ok) {
      food = {random(GRID_W), random(GRID_H)};
      ok = true;
      for (auto& p : snake) {
        if (p.x == food.x && p.y == food.y) { ok = false; break; }
      }
    }
  }
  
  void _reset() {
    snake.clear();
    snake.push_back({GRID_W/2,   GRID_H/2});
    snake.push_back({GRID_W/2-1, GRID_H/2});
    snake.push_back({GRID_W/2-2, GRID_H/2});
    dir = nextDir = 0;
    alive = true; score = 0; level = 1;
    speed = 200;
    _placeFood();
  }
  
  bool _moveSnake() {
    dir = nextDir;
    Point head = snake.front();
    Point nh = head;
    
    switch(dir) {
      case 0: nh.x++; break;
      case 1: nh.y++; break;
      case 2: nh.x--; break;
      case 3: nh.y--; break;
    }
    
    // Wall collision
    if (nh.x < 0 || nh.x >= GRID_W || nh.y < 0 || nh.y >= GRID_H) return false;
    
    // Self collision
    for (auto& p : snake) if (p.x == nh.x && p.y == nh.y) return false;
    
    snake.insert(snake.begin(), nh);
    
    if (nh.x == food.x && nh.y == food.y) {
      score += 10 * level;
      if (score > highScore) highScore = score;
      if (snake.size() % 5 == 0) {
        level++;
        speed = max(60, speed - 20);
      }
      _placeFood();
    } else {
      snake.pop_back();
    }
    return true;
  }
  
  void render(AppContext& ctx) {
    auto* spr = ctx.display->getContent();
    spr->fillSprite(CLR_BG);
    
    // Header
    spr->fillRect(0, 0, 240, 18, CLR_BG2);
    spr->setTextColor(CLR_WHITE); spr->setTextSize(1); spr->setTextDatum(ML_DATUM);
    spr->drawString("SNAKE", 5, 10);
    char scoreBuf[24];
    snprintf(scoreBuf, 24, "Score:%d Hi:%d Lv:%d", score, highScore, level);
    spr->setTextColor(CLR_YELLOW); spr->setTextDatum(MR_DATUM);
    spr->drawString(scoreBuf, 235, 10);
    
    // Grid border
    spr->drawRect(GRID_OX-1, GRID_OY-1,
      GRID_W*CELL_SZ+2, GRID_H*CELL_SZ+2, CLR_GRAY3);
    
    // Grid dots
    for (int gx = 0; gx < GRID_W; gx += 4) {
      for (int gy = 0; gy < GRID_H; gy += 4) {
        spr->drawPixel(GRID_OX + gx*CELL_SZ, GRID_OY + gy*CELL_SZ, CLR_GRAY3);
      }
    }
    
    // Food (pulsing)
    static int pulse = 0;
    static bool pdir = true;
    if (pdir) { if (++pulse >= 5) pdir = false; } else { if (--pulse <= 0) pdir = true; }
    
    int fx = GRID_OX + food.x * CELL_SZ;
    int fy = GRID_OY + food.y * CELL_SZ;
    spr->fillCircle(fx + CELL_SZ/2, fy + CELL_SZ/2, 4 + pulse/2, CLR_ACCENT);
    spr->fillCircle(fx + CELL_SZ/2, fy + CELL_SZ/2, 2, CLR_WHITE);
    
    // Snake body
    for (int i = 0; i < (int)snake.size(); i++) {
      int sx = GRID_OX + snake[i].x * CELL_SZ;
      int sy = GRID_OY + snake[i].y * CELL_SZ;
      
      float ratio = (float)i / snake.size();
      uint16_t c = spr->color565(
        0,
        (int)(200 - ratio * 100),
        (int)(50 + ratio * 100)
      );
      
      if (i == 0) {
        // Head
        spr->fillRoundRect(sx+1, sy+1, CELL_SZ-2, CELL_SZ-2, 3, CLR_GREEN);
        // Eyes
        int ex1 = sx + (dir == 0 ? 7 : dir == 2 ? 2 : 3);
        int ey1 = sy + (dir == 1 ? 7 : dir == 3 ? 2 : 2);
        int ex2 = sx + (dir == 0 ? 7 : dir == 2 ? 2 : 7);
        int ey2 = sy + (dir == 1 ? 7 : dir == 3 ? 2 : 7);
        spr->fillCircle(ex1, ey1, 1, CLR_BLACK);
        spr->fillCircle(ex2, ey2, 1, CLR_BLACK);
      } else {
        spr->fillRoundRect(sx+1, sy+1, CELL_SZ-2, CELL_SZ-2, 2, c);
      }
    }
    
    // Death screen
    if (!alive) {
      spr->fillRoundRect(40, 90, 160, 80, 10, CLR_BG2);
      spr->drawRoundRect(40, 90, 160, 80, 10, CLR_ACCENT);
      spr->setTextColor(CLR_ACCENT); spr->setTextSize(2); spr->setTextDatum(MC_DATUM);
      spr->drawString("GAME OVER", 120, 110);
      spr->setTextColor(CLR_WHITE); spr->setTextSize(1);
      char sb[20]; snprintf(sb, 20, "Score: %d", score);
      spr->drawString(sb, 120, 130);
      spr->setTextColor(CLR_GRAY2);
      spr->drawString("START: Play Again", 120, 148);
      spr->drawString("A: Exit", 120, 162);
    }
    
    if (!started) {
      spr->fillRoundRect(40, 100, 160, 60, 10, CLR_BG2);
      spr->setTextColor(CLR_GREEN); spr->setTextSize(2); spr->setTextDatum(MC_DATUM);
      spr->drawString("SNAKE", 120, 115);
      spr->setTextColor(CLR_GRAY1); spr->setTextSize(1);
      spr->drawString("Press A to start", 120, 138);
      spr->drawString("D-Pad to move", 120, 150);
    }
    
    // Speed bar
    spr->fillRect(0, 280, 240, 20, CLR_BG2);
    spr->setTextColor(CLR_GRAY2); spr->setTextDatum(ML_DATUM);
    spr->drawString("Speed:", 5, 289);
    UITheme::drawProgressBar(spr, 50, 285, 100, 8, map(speed, 60, 200, 100, 0), CLR_GREEN, CLR_GRAY3);
    spr->setTextColor(CLR_GRAY3); spr->setTextDatum(MR_DATUM);
    spr->drawString("A:Exit", 235, 289);
    
    ctx.display->pushContent();
  }
  
  void start(AppContext& ctx) {
    _reset();
    started = false;
  }
  
  void loop(AppContext& ctx) {
    render(ctx);
    
    if (!started) {
      if (ctx.input->start()) { started = true; lastMove = millis(); }
      if (ctx.input->b()) ctx.os->setState(OSState::HOME_SCREEN);
      return;
    }
    
    if (!alive) {
      if (ctx.input->start()) { _reset(); started = true; lastMove = millis(); }
      if (ctx.input->b()) ctx.os->setState(OSState::HOME_SCREEN);
      return;
    }
    
    // Direction input (prevent 180° reversal)
    if (ctx.input->right() && dir != 2) nextDir = 0;
    if (ctx.input->down()  && dir != 3) nextDir = 1;
    if (ctx.input->left()  && dir != 0) nextDir = 2;
    if (ctx.input->up()    && dir != 1) nextDir = 3;
    if (ctx.input->b()) { ctx.os->setState(OSState::HOME_SCREEN); return; }
    
    // Move snake at speed interval
    if (millis() - lastMove >= (uint32_t)speed) {
      lastMove = millis();
      alive = _moveSnake();
    }
  }
}
