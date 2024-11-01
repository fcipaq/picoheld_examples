/*
 * Pico Racer example for the Pico Held pplib library
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
 
#pragma GCC optimize("Ofast")

#include <pplib.h>
#include <fonts/fontfiles/f13x16.h>

#include "pico/stdlib.h" // overclock
#include "hardware/vreg.h" // overclock

#include <inttypes.h>
#include <math.h>
#include <stdlib.h>  // itoa()

/* ====================== assets ====================== */
#include "assets/mud_track.h"

#if LCD_COLORDEPTH == 16
  #include "assets/mud_track_tiles16.h"
  #include "assets/car_rear_data16.h"
  #include "assets/cockpit1_data16.h"
  #include "assets/cockpit2_data16.h"
#elif LCD_COLORDEPTH == 8
  #include "assets/mud_track_tiles8.h"
  #include "assets/car_rear_data8.h"
  #include "assets/cockpit1_data8.h"
  #include "assets/cockpit2_data8.h"
#endif

#define MUSIC
#ifdef MUSIC
  #include "assets/music.h"
  #include "assets/snd_low_grip.h"
  #include "assets/snd_motor_accel.h"
  #include "assets/snd_motor_idle.h"
#endif

/* ====================== variables ====================== */
uint8_t tile_factor = tiles_data.width / 64;  // normalize

bool double_buffering = false;

int v_theta_start = 90;
int v_theta_delta = 0;
int start_x = 2400, start_y = 340;

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
  if (ppl_init() != 0)
    blink_error(1);

  uint32_t sys_clock_khz = 252000; //276000, 252000, 133000
  if (sys_clock_khz != 133000) {
    vreg_set_voltage(VREG_VOLTAGE_1_20);
    sleep_ms(10);
    set_sys_clock_khz(sys_clock_khz, true);
    // recalculate PIO speed after overclocking
    lcd_set_speed(LCD_PIO_SPEED * 1000000);
  }

  lcd_set_backlight(40);
  snd_set_vol(5);
  snd_set_freq(44000);
}

void loop() {
  
  #if LCD_COLORDEPTH == 8
    // load palette
    color_palette_t* custom_palette = lcd_get_palette_ptr();

    for (int h = 0; h < 256; h++)
      custom_palette[h] = tiles_data_palette[h];
  #endif

  // prepare framebuffer
  gbuffer_t fb_back;
  if (gbuf_alloc(&fb_back, lcd_get_screen_width(), lcd_get_screen_height()) != BUF_SUCCESS)
    blink_error(2);

  // If there's enough memory, enable double buffering
  gbuffer_t fb1;

  if (gbuf_alloc(&fb1, lcd_get_screen_width(), lcd_get_screen_height()) == BUF_SUCCESS)
    double_buffering = true;

  gbuffer_t fb2 = fb_back;

  #if defined LCD_DOUBLE_PIXEL_LINEAR || defined LCD_DOUBLE_PIXEL_NEAREST
    float defaultzoom = 1.0f;
  #else
    float defaultzoom = 2.0f;
  #endif

  float a = 0.05f;  // acceleration
  float b = -0.2f;  // deceleration
  float v = 0.0f;   // velocity
  float v_max = 15;  // max. velocity

  float zoom = defaultzoom;
  float pos_x = start_x;  // position on map
  float pos_y = start_y;
  int v_theta = v_theta_start;  // orientation of car

  // fps counting
  uint32_t fps_timer = millis();
  uint16_t fps_cnt = 0;
  uint16_t fps = 0;

  uint8_t mode7 = 0;  // render mode
  uint8_t b1_deb = 0;  // button debounce

  while (1) {
    draw_rect_fill(0,
                   0, 
                   lcd_get_screen_width() - 1,
                   lcd_get_screen_height() - 1,
                   0x00,
                   fb_back);

    // controls
    uint16_t dpad = ctrl_dpad_state();
    uint16_t buttons = ctrl_button_state();

    if (buttons & BUTTON_1) {
      if (!b1_deb) {
        b1_deb = 1;
        mode7++;
        if (mode7 == 4)
          mode7 = 0;
      }
    } else {
      b1_deb = 0;
    }

    if (buttons & BUTTON_3) {
      v += a;
      if (v > v_max)
        v = v_max;
      #ifdef MUSIC
        if (snd_num_bufs_waiting(SND_CHAN_2) == 0) {
          if (v < v_max / 2)
            snd_enque_buf((uint8_t*) snd_motor_accel, snd_motor_accel_len, SND_CHAN_2, SND_NONBLOCKING);
          else
            snd_enque_buf((uint8_t*) snd_motor_idle, snd_motor_idle_len, SND_CHAN_2, SND_NONBLOCKING);
        }
      #endif
    }

    if (buttons & BUTTON_2) {
      v += b;
      if (v < 0)
        v = 0;
    }

    if (!buttons) {
      v += b / 10;
      if (v < 0)
        v = 0;
    }

    // motion calculation
    uint8_t steering = 0;

    if ((v >= 0.3f) && (dpad & DPAD_LEFT)) {
      v_theta += 2;
      steering = 1;
    }

    if ((v >= 0.3f) && (dpad & DPAD_RIGHT)) {
      v_theta -= 2;
      steering = 1;
    }

    if (steering)
      v_theta_delta += 1;
    else
      v_theta_delta--;

    if (v_theta_delta > 50)
      v_theta_delta = 50;
    if (v_theta_delta < 0)
      v_theta_delta = 0;

    #ifdef MUSIC
      if ((v_theta_delta == 50) && (snd_num_bufs_waiting(SND_CHAN_1) == 0))
        snd_enque_buf((uint8_t*)snd_low_grip, snd_low_grip_len, SND_CHAN_1, SND_NONBLOCKING);
    #endif

    if (v_theta > 360)
      v_theta -= 360;
    if (v_theta < 0)
      v_theta += 360;

    // calculate new position
    float theta = v_theta * M_PI / 180;
    pos_x += v * sinf(theta);
    pos_y += v * cosf(theta);

    if ((pos_x > map_data.width * tiles_data.width) ||
        (pos_x < 0 * tiles_data.width) ||
        (pos_y > map_data.height * tiles_data.height) ||
        (pos_y < 0 * tiles_data.height)) {
      pos_x = start_x;
      pos_y = start_y;
      v = 0;
      v_theta = v_theta_start;
    }

    zoom = (defaultzoom / (v / 8. + 1.)); // 10

    // rendering
    if (mode7 == 0)
      zoom /= 2;

    coord_t tm_x;
    coord_t tm_y;
    coord_t tm_w;
    coord_t tm_h;

    if ((mode7 == 0) || (mode7 == 1)) {
      // top down view (car rotating)
      if (mode7 == 1) {
        tm_x = 0;
        tm_y = 0;
        tm_w = lcd_get_screen_width();
        tm_h = lcd_get_screen_height();
      } else {
        tm_x = 0;
        tm_y = 0;
        tm_w = lcd_get_screen_width() / 2;
        tm_h = lcd_get_screen_height() / 2;
      }

      tile_blit(tm_x,
                tm_y,
                tm_w,
                tm_h,
                pos_x - tm_w / 2. / (zoom / tile_factor),  // pivot in centre
                pos_y - tm_h / 2. / (zoom / tile_factor),  // pivot in centre
                zoom / tile_factor,
                zoom / tile_factor,
                map_data,
                tiles_data,
                BLIT_NO_ALPHA,
                fb_back);

      #if LCD_COLORDEPTH == 16
        color_t alpha = 0xf81f;
      #elif LCD_COLORDEPTH == 8
        color_t alpha = 0xe2;
      #endif

      blit_buf(tm_x + tm_w / 2,
               tm_y + tm_h / 2,
               zoom / 2,
               (v_theta + 180) * M_PI / 180,
               alpha,
               car_data,
               fb_back);
    }

    if ((mode7 == 0) || (mode7 == 2)) {
      // top down view (map rotating)
      if (mode7 == 2) {
        tm_x = 0;
        tm_y = 0;
        tm_w = lcd_get_screen_width();
        tm_h = lcd_get_screen_height();
      } else {
        tm_x = 0;
        tm_y = lcd_get_screen_height() / 2;
        tm_w = lcd_get_screen_width() / 2;
        tm_h = lcd_get_screen_height() / 2;
      }

      tile_blit_rot(tm_x,
                    tm_y,
                    tm_w,
                    tm_h,
                    pos_x,
                    pos_y,
                    tm_w / 2.,
                    tm_h / 2.,
                    (180 - v_theta) * M_PI / 180.0f,
                    zoom / tile_factor,
                    zoom / tile_factor,
                    map_data,
                    tiles_data,
                    BLIT_NO_ALPHA,
                    fb_back);

      #if LCD_COLORDEPTH == 16
        color_t alpha = 0xf81f;
      #elif LCD_COLORDEPTH == 8
        color_t alpha = 0xe2;
      #endif
      
      blit_buf(tm_x + tm_w / 2,
               tm_y + tm_h / 2,
               zoom / 2, 0,
               alpha,
               car_data,
               fb_back);

    }

    if ((mode7 == 0) || (mode7 == 3)) {
      // "mode7" pseudo 3D
      uint8_t sky_height = 30;

      if (mode7 == 3) {
        tm_x = 0;
        tm_y = 0;
        tm_w = lcd_get_screen_width();
        tm_h = lcd_get_screen_height() - 10;
      } else {
        tm_x = lcd_get_screen_width() / 2;
        tm_y = 0;
        tm_w = lcd_get_screen_width() / 2;
        tm_h = lcd_get_screen_height() / 2;
      }

      int a = (mode7 == 0) ? 2 : 1;

      tile_blit_mode7(tm_x,
                      tm_y + sky_height / a,
                      tm_w,
                      tm_h - sky_height / a,
                      pos_x,
                      pos_y,
                      20,  // gets divided by 100 in function
                      v_theta * M_PI / 180.0f,
                      map_data,
                      tiles_data,
                      BLIT_NO_ALPHA,
                      fb_back);
      
      #if LCD_COLORDEPTH == 16
        color_t sky_color = rgb_col_888_565(0, 128, 180);
      #elif LCD_COLORDEPTH == 8
        color_t sky_color = 0xe2; // blue in palette
      #endif
      
      draw_rect_fill(tm_x,
                     tm_y,
                     tm_x + tm_w - 1,
                     tm_y + (sky_height + 4) / a,
                     sky_color,
                     fb_back);

      #if LCD_COLORDEPTH == 16
        color_t alpha = 0xf81f;
      #elif LCD_COLORDEPTH == 8
        color_t alpha = 0xe3;      
      #endif

      if (mode7 == 3) {
        blit_buf(0,
                 0,
                 alpha,
                 cockpit2_data,
                 fb_back);
        blit_buf(0,
                 171,
                 alpha,
                 cockpit1_data,
                 fb_back);
      }

      if (mode7 == 0) {

        #if LCD_COLORDEPTH == 16
          color_t col = rgb_col_888_565(255, 255, 255);
        #elif LCD_COLORDEPTH == 8
          color_t col = rgb_col_888_332(255, 255, 255);
        #endif

        draw_rect_fill(lcd_get_screen_width() / 2 - 1,
                       0,
                       lcd_get_screen_width() / 2 + 1,
                       lcd_get_screen_height(),
                       col,
                       fb_back);

        draw_rect_fill(0,
                       lcd_get_screen_height() / 2 - 1,
                       lcd_get_screen_width(),
                       lcd_get_screen_height() / 2 + 1,
                       col,
                       fb_back);
      }
    
    }

    // fps counter
    fps_cnt++;
    if (millis() - fps_timer > 2000) {
      fps = fps_cnt * 1000 / (millis() - fps_timer);
      fps_cnt = 0;
      fps_timer = millis();
    }

    char debug_print[11];
    itoa(fps, debug_print, 10);
    font_write_string(10, 10, 0xff, debug_print, (font_t*) font_13x16, fb_back);
    font_write_string(9, 9, 0x0000, debug_print, (font_t*) font_13x16, fb_back);

    #ifdef MUSIC
      snd_enque_buf((uint8_t*)snd_data, snd_file_len, SND_CHAN_0, SND_NONBLOCKING);
    #endif
 
    // vsync
    lcd_wait_ready();
    while (lcd_get_vblank());
    
    // show display
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
