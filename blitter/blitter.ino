/*
 * Blitter example for the Pico Held pplib library
 *
 * Copyright (C) 2023 Daniel Kammer (daniel.kammer@web.de)
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
 *
 */
 
#include <pplib.h>

#include <fonts/f13x16.h>
#include "lcdcom.h"

#include <stdlib.h>  // itoa()

/* ====================== assets ====================== */
// Assets by freepik.com
#include "assets/smiley_8.h"
#include "assets/smiley2_8.h"

#if LCD_COLORDEPTH!=8
#error Color depth needs to be 8 bits. Please set in setup.h
#endif

/* ==================== types ==================== */

typedef struct 
{
  int x;   // position x
  int y;   // position y
  int z;   // zoom
  int r;   // rotation
  int dx;  // delta x
  int dy;  // delta y
  int dz;  // delta zoom
  int dr;  // delta rotation

  int zx;  // stretch x
  int zy;  // stretch y
  int dzx; // delta stretch x
  int dzy; // delta stretch y
} smiley_t;

/* ==================== implementation ==================== */

void blink_error(byte n) {
  pinMode(LED_BUILTIN, OUTPUT);
  while (1) {
    for (byte h = 0; h < n; h++) {
      digitalWrite(LED_BUILTIN, HIGH);
      delay(500);
      digitalWrite(LED_BUILTIN, LOW);
      delay(500);
    }
    delay(2000);
  }
}

void setup() {
  if (ppl_init() != PPL_SUCCESS)
    blink_error(1);

  // set backlight level to 10%
  lcd_set_backlight(10);

  // silence machine
  snd_set_vol(0);
  snd_set_freq(44000);
}

void loop() { 
  // fps counter
  uint32_t fps_timer = millis();
  uint16_t fps = 0;
  uint16_t fps_cnt = 0;
  
  // prepare framebuffer
  gbuffer_t fb_back;
  if (gbuf_alloc(&fb_back, lcd_get_screen_width(), lcd_get_screen_height(), LCD_COLORDEPTH) != BUF_SUCCESS)
    blink_error(2);

  // If there's enough memory, enable double buffering
  gbuffer_t fb1;
  bool double_buffering = false;
  if (gbuf_alloc(&fb1, lcd_get_screen_width(), lcd_get_screen_height(), LCD_COLORDEPTH) == BUF_SUCCESS)
    double_buffering = true;
  
  gbuffer_t fb2 = fb_back;

  // load palette
  color_palette_t* custom_palette = lcd_get_palette_ptr();

  for (int h = 0; h < 256; h++)
    custom_palette[h] = smiley_palette_data[h];

  // set no. of sprites
  int num_sprites = 16;
  
  smiley_t smiley[2][num_sprites / 2];

  // initialize sprite object values
  for (int h = 0; h < num_sprites / 2; h++) {
    smiley[0][h].x = random(lcd_get_screen_width());
    smiley[0][h].y = random(lcd_get_screen_height());
    smiley[0][h].r = random(360);
    smiley[0][h].z = random(200) + 50;
    smiley[0][h].dz = random(7) - 3;
    smiley[0][h].dx = random(7) - 3;
    smiley[0][h].dy = random(7) - 3;
    smiley[0][h].dr = random(7) - 3;

    smiley[1][h].x = random(lcd_get_screen_width());
    smiley[1][h].y = random(lcd_get_screen_height());
    smiley[1][h].zx = random(200) + 50;
    smiley[1][h].zy = random(200) + 50;
    smiley[1][h].dx = random(7) - 3;
    smiley[1][h].dy = random(7) - 3;
    smiley[1][h].dzx = random(7) - 3;
    smiley[1][h].dzy = random(7) - 3;
  }
  
  while (1) {

    draw_rect_fill(0, 0, lcd_get_screen_width() - 1, lcd_get_screen_width() - 1, 0xff, fb_back);

    for (int h = 0; h < num_sprites / 2; h++) {
      // smiley 1 (the rotating, laughing one)
      smiley[0][h].x += smiley[0][h].dx;
      smiley[0][h].y += smiley[0][h].dy;
      smiley[0][h].z += smiley[0][h].dz;
      smiley[0][h].r += smiley[0][h].dr;

      if ((smiley[0][h].x < 0) || smiley[0][h].x > lcd_get_screen_width() - 1)
        smiley[0][h].dx = -smiley[0][h].dx;
      if ((smiley[0][h].y < 0) || smiley[0][h].y > lcd_get_screen_height() - 1)
        smiley[0][h].dy = -smiley[0][h].dy;
      if ((smiley[0][h].z < 50) || smiley[0][h].z > 249)
        smiley[0][h].dz = -smiley[0][h].dz;
      if (smiley[0][h].r < 0)
        smiley[0][h].r += 360;
      if (smiley[0][h].r > 359)
        smiley[0][h].r -= 360;

      // rotation
      blit_buf(smiley[0][h].x, smiley[0][h].y, (float) smiley[0][h].z / 300., smiley[0][h].r * M_PI / 180, 0xcd, smiley_data, fb_back);

      // smiley 2 (the other one being stretched)
      if ((smiley[1][h].x < 0) || smiley[1][h].x > lcd_get_screen_width() - 1)
        smiley[1][h].dx = -smiley[1][h].dx;
      if ((smiley[1][h].y < 0) || smiley[1][h].y > lcd_get_screen_height() - 1)
        smiley[1][h].dy = -smiley[1][h].dy;
      if ((smiley[1][h].zx + smiley[1][h].dzx < 50) || smiley[1][h].zx + smiley[1][h].dzx > 249)
        smiley[1][h].dzx = -smiley[1][h].dzx;
      if ((smiley[1][h].zy + smiley[1][h].dzy < 50) || smiley[1][h].zy + smiley[1][h].dzy > 249)
        smiley[1][h].dzy = -smiley[1][h].dzy;

      smiley[1][h].x += smiley[1][h].dx;
      smiley[1][h].y += smiley[1][h].dy;
      smiley[1][h].zx += smiley[1][h].dzx;
      smiley[1][h].zy += smiley[1][h].dzy;

      // x/y-zooming
      blit_buf(smiley[1][h].x, smiley[1][h].y, (float) smiley[1][h].zx / 500., (float) smiley[1][h].zy / 500., BLIT_FLIP_NONE, 0xcd, smiley2_data, fb_back);
    }

    // frame rate counter
    fps_cnt++;
    if (millis() - fps_timer > 2000) {
      fps = fps_cnt * 1000 / (millis() - fps_timer);
      fps_cnt = 0;
      fps_timer = millis();
    }

    char debug_print[11];
    itoa(fps, debug_print, 10);

    font_write_string(10, 10, 0xff, debug_print, (font_t*) font_13x16, fb_back);
    font_write_string(9, 9, 0x00, debug_print, (font_t*) font_13x16, fb_back);

    // wait for vlbank
    lcd_wait_ready();
    while (lcd_get_vblank());

    lcd_show_framebuffer(fb_back);

    if (!double_buffering) {
      lcd_wait_ready();
    } else {
      if (fb_back.data == fb2.data)
        fb_back = fb1;
      else
        fb_back = fb2;
    }
  } // main loop

} // loop()
