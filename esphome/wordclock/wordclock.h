// Word clock logic: time -> words or clock hands, color sets, timer display, notification
// animations, all rendered into one LED frame.
//
// Deliberately free of ESPHome/Arduino dependencies so it can be unit tested on a PC (see test/).
// The ESPHome configuration (core.yaml) only gathers the settings and copies the finished frame
// to the LED strip.
//
// LED layout (derived from the word indices of the "Wortuhr V3" sketch by Mirc0):
//   10 rows x 11 letters of WS2812B wired as a serpentine:
//   even rows run left -> right, odd rows right -> left.
//   The 4 minute dots follow as LEDs 110-113.
//
//      col  0 1 2 3 4 5 6 7 8 9 10
//   row 0   E S K I S T A F Ü N F
//       1   Z E H N Z W A N Z I G
//       2   D R E I V I E R T E L
//       3   V O R W I F I N A C H     (W I F I = WiFi status field)
//       4   H A L B A E L F Ü N F
//       5   E I N S X A M Z W E I
//       6   D R E I A U J V I E R
//       7   S E C H S N L A C H T
//       8   S I E B E N Z W Ö L F
//       9   Z E H N E U N K U H R
//   Filler letters (K, A, X, AM, AUJ, NL, K) are never lit by the time, only by timers and animations.
#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace wordclock {

constexpr int ROWS = 10;
constexpr int COLS = 11;
constexpr int NUM_LETTERS = ROWS * COLS;  // 110
constexpr int NUM_DOTS = 4;
constexpr int NUM_LEDS = NUM_LETTERS + NUM_DOTS;  // 114
constexpr int FIRST_DOT = NUM_LETTERS;

// The last timer minute: exactly this many letters are lit and one goes out per second.
constexpr uint32_t FINAL_PHASE_S = 60;
constexpr uint32_t FINAL_PHASE_MS = FINAL_PHASE_S * 1000;

struct RGB {
  uint8_t r, g, b;
  bool operator==(const RGB &o) const { return r == o.r && g == o.g && b == o.b; }
  bool operator!=(const RGB &o) const { return !(*this == o); }
};
constexpr RGB BLACK{0, 0, 0};

using Frame = std::array<RGB, NUM_LEDS>;
using Mask = std::array<bool, NUM_LEDS>;

// Row/column as seen from the front -> LED index on the strip.
constexpr int led_index(int row, int col) {
  return row * COLS + (row % 2 == 0 ? col : COLS - 1 - col);
}

struct Word {
  uint8_t row, col, len;
};

// Word positions on the front (the identifiers are the German words they spell)
constexpr Word ES{0, 0, 2};
constexpr Word IST{0, 3, 3};
constexpr Word FUENF_M{0, 7, 4};
constexpr Word ZEHN_M{1, 0, 4};
constexpr Word ZWANZIG{1, 4, 7};
constexpr Word DREIVIERTEL{2, 0, 11};
constexpr Word VIERTEL{2, 4, 7};
constexpr Word VOR{3, 0, 3};
constexpr Word WIFI{3, 3, 4};
constexpr Word NACH{3, 7, 4};
constexpr Word HALB{4, 0, 4};
constexpr Word ELF{4, 5, 3};
constexpr Word FUENF_H{4, 7, 4};
constexpr Word EINS{5, 0, 4};
constexpr Word EIN{5, 0, 3};
constexpr Word ZWEI{5, 7, 4};
constexpr Word DREI{6, 0, 4};
constexpr Word VIER{6, 7, 4};
constexpr Word SECHS{7, 0, 5};
constexpr Word ACHT{7, 7, 4};
constexpr Word SIEBEN{8, 0, 6};
constexpr Word ZWOELF{8, 6, 5};
constexpr Word ZEHN_H{9, 0, 4};
constexpr Word NEUN{9, 3, 4};
constexpr Word UHR{9, 8, 3};

// Indexed by hour 1..12
constexpr Word HOURS[13] = {ZWOELF, EINS, ZWEI, DREI, VIER, FUENF_H, SECHS,
                            SIEBEN, ACHT, NEUN, ZEHN_H, ELF, ZWOELF};

enum class Dialect : uint8_t {
  WEST = 0,  // "Viertel nach zwei", "Viertel vor drei" (most of Germany, Austria, Switzerland)
  EAST = 1,  // "Viertel drei", "Dreiviertel drei" (eastern and southern regions)
};

inline void add(Mask &m, const Word &w) {
  for (int c = w.col; c < w.col + w.len; c++)
    m[led_index(w.row, c)] = true;
}

// Which LEDs show the time: words in 5 minute steps plus 0-4 minute dots.
inline Mask time_mask(int hour, int minute, Dialect dialect) {
  Mask m{};
  add(m, ES);
  add(m, IST);

  const int rounded = (minute / 5) * 5;
  const int extra = minute % 5;

  // From "fünf vor halb" (xx:25) on, German refers to the next hour
  int h = hour % 12;
  if (h == 0)
    h = 12;
  if (rounded >= 25)
    h = h % 12 + 1;
  const Word &hw = HOURS[h];

  switch (rounded) {
    case 0:
      add(m, h == 1 ? EIN : hw);  // "EIN UHR", not "EINS UHR"
      add(m, UHR);
      break;
    case 5:
      add(m, FUENF_M), add(m, NACH), add(m, hw);
      break;
    case 10:
      add(m, ZEHN_M), add(m, NACH), add(m, hw);
      break;
    case 15:
      if (dialect == Dialect::EAST)
        add(m, VIERTEL), add(m, HOURS[h % 12 + 1]);
      else
        add(m, VIERTEL), add(m, NACH), add(m, hw);
      break;
    case 20:
      add(m, ZWANZIG), add(m, NACH), add(m, hw);
      break;
    case 25:
      add(m, FUENF_M), add(m, VOR), add(m, HALB), add(m, hw);
      break;
    case 30:
      add(m, HALB), add(m, hw);
      break;
    case 35:
      add(m, FUENF_M), add(m, NACH), add(m, HALB), add(m, hw);
      break;
    case 40:
      add(m, ZWANZIG), add(m, VOR), add(m, hw);
      break;
    case 45:
      if (dialect == Dialect::EAST)
        add(m, DREIVIERTEL), add(m, hw);
      else
        add(m, VIERTEL), add(m, VOR), add(m, hw);
      break;
    case 50:
      add(m, ZEHN_M), add(m, VOR), add(m, hw);
      break;
    case 55:
      add(m, FUENF_M), add(m, VOR), add(m, hw);
      break;
  }

  for (int i = 0; i < extra; i++)
    m[FIRST_DOT + i] = true;
  return m;
}

// ---------------------------------------------------------------------------
// Timer
// ---------------------------------------------------------------------------

enum class TimerState : uint8_t { IDLE, RUNNING, RINGING };

// All times are milliseconds of a monotonic clock (millis()); the 49-day wrap-around is handled
// by computing signed differences.
class Timer {
 public:
  // remaining = time left, total = original duration (used for the progress before the last minute).
  void start(uint32_t remaining_ms, uint32_t total_ms, uint32_t now_ms) {
    if (remaining_ms == 0) {
      cancel();
      return;
    }
    state_ = TimerState::RUNNING;
    end_ms_ = now_ms + remaining_ms;
    total_ms_ = total_ms < remaining_ms ? remaining_ms : total_ms;
  }

  void cancel() { state_ = TimerState::IDLE; }

  // Call regularly. Returns true when the state changed.
  // ring_timeout_ms = 0 -> keeps ringing until the timer is stopped.
  bool update(uint32_t now_ms, uint32_t ring_timeout_ms) {
    if (state_ == TimerState::RUNNING && remaining_ms(now_ms) == 0) {
      state_ = TimerState::RINGING;
      ring_start_ms_ = now_ms;
      return true;
    }
    if (state_ == TimerState::RINGING && ring_timeout_ms > 0 &&
        now_ms - ring_start_ms_ >= ring_timeout_ms) {
      state_ = TimerState::IDLE;
      return true;
    }
    return false;
  }

  TimerState state() const { return state_; }
  uint32_t total_ms() const { return total_ms_; }
  uint32_t ring_elapsed_ms(uint32_t now_ms) const { return now_ms - ring_start_ms_; }

  uint32_t remaining_ms(uint32_t now_ms) const {
    if (state_ != TimerState::RUNNING)
      return 0;
    const int32_t diff = static_cast<int32_t>(end_ms_ - now_ms);
    return diff > 0 ? static_cast<uint32_t>(diff) : 0;
  }

  // Rounded up: 0.3 s left -> 1, so that "1" is the last letter shown.
  uint32_t remaining_s(uint32_t now_ms) const { return (remaining_ms(now_ms) + 999) / 1000; }

 private:
  TimerState state_{TimerState::IDLE};
  uint32_t end_ms_{0};
  uint32_t total_ms_{0};
  uint32_t ring_start_ms_{0};
};

// How many of the free letters the timer lights:
//   > 60 s:  progress bar, shrinks linearly from "all free letters" to 60
//   <= 60 s: exactly one letter per remaining second
inline int timer_letter_count(uint32_t remaining_ms, uint32_t total_ms, int available) {
  if (remaining_ms == 0)
    return 0;
  if (remaining_ms <= FINAL_PHASE_MS) {
    const int n = static_cast<int>((remaining_ms + 999) / 1000);
    return n > available ? available : n;
  }
  if (available <= static_cast<int>(FINAL_PHASE_S) || total_ms <= FINAL_PHASE_MS)
    return available;
  const uint64_t span = available - FINAL_PHASE_S;
  const uint64_t extra = span * (remaining_ms - FINAL_PHASE_MS) / (total_ms - FINAL_PHASE_MS);
  const int n = static_cast<int>(FINAL_PHASE_S + extra);
  return n > available ? available : n;
}

// ---------------------------------------------------------------------------
// Color sets
// ---------------------------------------------------------------------------

constexpr int MAX_PALETTE_COLORS = 6;

struct Palette {
  uint8_t count;
  RGB colors[MAX_PALETTE_COLORS];
};

// Order must match the "Color Set" select in core.yaml (which starts with "Single Color").
constexpr Palette PALETTES[] = {
    {6, {{255, 0, 0}, {255, 200, 0}, {0, 255, 0}, {0, 220, 255}, {0, 0, 255}, {255, 0, 200}}},  // Rainbow
    {4, {{255, 60, 0}, {255, 140, 20}, {255, 30, 90}, {150, 30, 180}}},                         // Sunset
    {4, {{0, 60, 255}, {0, 180, 255}, {0, 255, 180}, {60, 100, 255}}},                          // Ocean
    {4, {{20, 200, 40}, {120, 220, 20}, {0, 160, 90}, {200, 180, 30}}},                         // Forest
    {3, {{255, 30, 0}, {255, 90, 0}, {255, 170, 10}}},                                          // Fire
    {4, {{255, 130, 170}, {150, 190, 255}, {150, 255, 190}, {255, 230, 130}}},                  // Pastel
    {3, {{0, 255, 130}, {0, 180, 255}, {150, 60, 255}}},                                        // Aurora
};
constexpr int NUM_PALETTES = sizeof(PALETTES) / sizeof(PALETTES[0]);

inline RGB mix(RGB a, RGB b, uint8_t amount) {
  auto ch = [amount](uint8_t x, uint8_t y) {
    return static_cast<uint8_t>((x * (255 - amount) + y * amount) / 255);
  };
  return {ch(a.r, b.r), ch(a.g, b.g), ch(a.b, b.b)};
}

// pos = position on the color wheel, 0..65535 = once through all colors
inline RGB palette_color(const Palette &p, uint16_t pos) {
  const uint32_t scaled = static_cast<uint32_t>(pos) * p.count;
  const int i = scaled >> 16;
  return mix(p.colors[i], p.colors[(i + 1) % p.count], static_cast<uint8_t>(scaled >> 8));
}

// ---------------------------------------------------------------------------
// Geometry, analog clock
// ---------------------------------------------------------------------------

// Center of the front (between rows 4 and 5); row and column pitch are assumed to be equal.
constexpr float CENTER_COL = 5.0f;
constexpr float CENTER_ROW = 4.5f;
constexpr float TWO_PI_F = 6.28318530718f;

constexpr int led_row(int idx) { return idx / COLS; }
constexpr int led_col(int idx) {
  return idx / COLS % 2 == 0 ? idx % COLS : COLS - 1 - idx % COLS;
}
constexpr bool is_border(int idx) {
  return led_row(idx) == 0 || led_row(idx) == ROWS - 1 || led_col(idx) == 0 || led_col(idx) == COLS - 1;
}

// Angle of every letter: 0..65535 clockwise from 12 o'clock
inline const std::array<uint16_t, NUM_LETTERS> &letter_angles() {
  static const std::array<uint16_t, NUM_LETTERS> table = [] {
    std::array<uint16_t, NUM_LETTERS> t{};
    for (int i = 0; i < NUM_LETTERS; i++) {
      float a = atan2f(led_col(i) - CENTER_COL, CENTER_ROW - led_row(i));
      if (a < 0)
        a += TWO_PI_F;
      t[i] = static_cast<uint16_t>(a / TWO_PI_F * 65535.0f);
    }
    return t;
  }();
  return table;
}

// The border letter closest to an angle
inline int nearest_border_led(uint16_t angle) {
  const auto &angles = letter_angles();
  int best = 0, best_diff = 70000;
  for (int i = 0; i < NUM_LETTERS; i++) {
    if (!is_border(i))
      continue;
    int diff = static_cast<int16_t>(static_cast<uint16_t>(angles[i] - angle));
    if (diff < 0)
      diff = -diff;
    if (diff < best_diff) {
      best_diff = diff;
      best = i;
    }
  }
  return best;
}

// Per LED: 0 = not part of the time, otherwise the number of its word or hand (from 1).
// With a color set, every word gets its own hue through this number.
using Groups = std::array<uint8_t, NUM_LEDS>;

// Adjacent lit letters within a row form a word. Returns the number of words.
inline int word_groups(const Mask &m, Groups &g) {
  int n = 0;
  for (int r = 0; r < ROWS; r++) {
    bool in_word = false;
    for (int c = 0; c < COLS; c++) {
      const int i = led_index(r, c);
      if (m[i]) {
        if (!in_word)
          n++;
        g[i] = static_cast<uint8_t>(n);
        in_word = true;
      } else {
        in_word = false;
      }
    }
  }
  bool dots = false;
  for (int i = FIRST_DOT; i < NUM_LEDS; i++)
    if (m[i]) {
      if (!dots)
        n++;
      dots = true;
      g[i] = static_cast<uint8_t>(n);
    }
  return n;
}

inline void add_hand(Groups &g, float angle, float length, uint8_t group) {
  const float dx = sinf(angle), dy = -cosf(angle);
  for (float d = 0; d <= length; d += 0.4f) {
    const int col = static_cast<int>(floorf(CENTER_COL + d * dx + 0.5f));
    const int row = static_cast<int>(floorf(CENTER_ROW + d * dy + 0.5f));
    if (row >= 0 && row < ROWS && col >= 0 && col < COLS)
      g[led_index(row, col)] = group;
  }
}

// Hour hand (1), minute hand (2), seconds dot on the border (3).
// As with the words, the four dots show the minutes since the last 5 minute step.
inline int analog_groups(int hour, int minute, int second, bool seconds_hand, Groups &g) {
  const float m = minute + second / 60.0f;
  const float h = hour % 12 + m / 60.0f;
  add_hand(g, h / 12.0f * TWO_PI_F, 2.8f, 1);
  add_hand(g, m / 60.0f * TWO_PI_F, 4.6f, 2);
  for (int i = 0; i < minute % 5; i++)
    g[FIRST_DOT + i] = 2;
  if (!seconds_hand)
    return 2;
  g[nearest_border_led(static_cast<uint16_t>(second * 65536 / 60))] = 3;
  return 3;
}

// ---------------------------------------------------------------------------
// Rendering
// ---------------------------------------------------------------------------

enum class Display : uint8_t { WORDS = 0, ANALOG = 1 };

// How the countdown is drawn
enum class TimerStyle : uint8_t {
  LETTERS = 0,  // letters disappear from the bottom right, one per second in the last minute
  RING = 1,     // the border lights up and winds back to 12 o'clock like a clock hand
  PIE = 2,      // same as a pie slice across the whole front
};

struct Style {
  RGB clock_color{255, 225, 200};
  RGB timer_color{255, 60, 0};
  const Palette *palette{nullptr};   // nullptr = single color (clock_color)
  uint32_t palette_period_ms{60000};  // time for one pass through all colors
  Display display{Display::WORDS};
  bool seconds_hand{true};
  TimerStyle timer_style{TimerStyle::LETTERS};
  uint8_t prelude_level{80};  // timer brightness before the last minute, 0-255
  uint8_t level{255};         // overall level (night mode), 0-255
  Dialect dialect{Dialect::WEST};
  bool show_wifi_hint{false};  // pulse the "WIFI" field blue while WiFi is down
  uint32_t blink_half_period_ms{500};
};

inline RGB scale(RGB c, uint8_t level) {
  return {static_cast<uint8_t>(c.r * level / 255), static_cast<uint8_t>(c.g * level / 255),
          static_cast<uint8_t>(c.b * level / 255)};
}

// Color of a word or hand: with a color set, groups are spread evenly over the palette and drift slowly
inline RGB group_color(const Style &s, int group, int groups, uint32_t now_ms) {
  if (s.palette == nullptr)
    return s.clock_color;
  const uint32_t period = s.palette_period_ms == 0 ? 1 : s.palette_period_ms;
  const uint32_t time_pos = static_cast<uint32_t>((now_ms % period) * 65536ULL / period);
  const uint32_t group_pos = groups > 0 ? (group - 1) * 65536u / groups : 0;
  return palette_color(*s.palette, static_cast<uint16_t>(time_pos + group_pos));
}

inline void draw_timer(Frame &f, const Groups &g, const Timer &timer, uint32_t now_ms, const Style &s) {
  const uint32_t rem = timer.remaining_ms(now_ms);
  const RGB c = rem > FINAL_PHASE_MS ? scale(s.timer_color, s.prelude_level) : s.timer_color;

  if (s.timer_style == TimerStyle::LETTERS) {
    // Free letters in reading order; the first n are lit,
    // so the letters at the bottom right disappear first.
    std::array<uint8_t, NUM_LETTERS> free_leds{};
    int available = 0;
    for (int row = 0; row < ROWS; row++)
      for (int col = 0; col < COLS; col++) {
        const int idx = led_index(row, col);
        if (g[idx] == 0)
          free_leds[available++] = static_cast<uint8_t>(idx);
      }
    const int n = timer_letter_count(rem, timer.total_ms(), available);
    for (int i = 0; i < n; i++)
      f[free_leds[i]] = c;
    return;
  }

  // Circle: one full turn over the time before the last minute, then another full turn for the last minute.
  const uint32_t total = timer.total_ms();
  const uint32_t sweep = rem > FINAL_PHASE_MS
                             ? static_cast<uint32_t>((rem - FINAL_PHASE_MS) * 65536ULL / (total - FINAL_PHASE_MS))
                             : static_cast<uint32_t>(rem * 65536ULL / FINAL_PHASE_MS);
  const auto &angles = letter_angles();
  for (int i = 0; i < NUM_LETTERS; i++) {
    if (s.timer_style == TimerStyle::RING && !is_border(i))
      continue;
    if (angles[i] < sweep)
      f[i] = c;
  }
}

// time_valid = false: no time yet, so no words or hands are drawn.
inline Frame render(int hour, int minute, int second, bool time_valid, const Timer &timer, uint32_t now_ms,
                    const Style &s) {
  Frame f;
  f.fill(BLACK);

  Groups g{};
  int groups = 0;
  if (time_valid) {
    if (s.display == Display::ANALOG)
      groups = analog_groups(hour, minute, second, s.seconds_hand, g);
    else
      groups = word_groups(time_mask(hour, minute, s.dialect), g);
  }

  if (s.show_wifi_hint) {
    // slow pulse (2 s period)
    const uint32_t p = now_ms % 2000;
    const uint8_t v = static_cast<uint8_t>(30 + (p < 1000 ? p : 2000 - p) * 150 / 1000);
    for (int c = WIFI.col; c < WIFI.col + WIFI.len; c++)
      f[led_index(WIFI.row, c)] = RGB{0, static_cast<uint8_t>(v / 3), v};
  }

  // Dial: 12 faint hour marks
  if (time_valid && s.display == Display::ANALOG) {
    const RGB mark = scale(group_color(s, 1, groups, now_ms), 40);
    for (int h = 0; h < 12; h++)
      f[nearest_border_led(static_cast<uint16_t>(h * 65536 / 12))] = mark;
  }

  switch (timer.state()) {
    case TimerState::IDLE:
      break;

    case TimerState::RUNNING:
      draw_timer(f, g, timer, now_ms, s);
      break;

    case TimerState::RINGING: {
      // Everything blinks in the timer color; the time stays visible throughout.
      const bool on = (timer.ring_elapsed_ms(now_ms) / s.blink_half_period_ms) % 2 == 0;
      if (on)
        for (int i = 0; i < NUM_LEDS; i++)
          f[i] = s.timer_color;
      break;
    }
  }

  // The time is always drawn on top and shines through.
  for (int i = 0; i < NUM_LEDS; i++)
    if (g[i] != 0)
      f[i] = group_color(s, g[i], groups, now_ms);

  if (s.level != 255)
    for (auto &c : f)
      c = scale(c, s.level);
  return f;
}

// ---------------------------------------------------------------------------
// Notification animations
// ---------------------------------------------------------------------------
// Drawn in code rather than stored as bitmaps: on the ESP8266 constant data ends up in scarce RAM.
// To add one: extend the enum, ANIMATION_NAMES and ANIMATION_CYCLE_MS, write a draw_...() function,
// add it to render_animation() and add the option to the "Animation" select in core.yaml (same order).

enum class Animation : uint8_t { DOOR = 0, DOORBELL, PRINTER, WASHER, MAIL, DONE, WARNING, COUNT };

// Names used by the animation_show API action
constexpr const char *ANIMATION_NAMES[] = {"door", "doorbell", "printer", "washer", "mail", "done", "warning"};

// Length of one cycle
constexpr uint32_t ANIMATION_CYCLE_MS[] = {5500, 3000, 8000, 8000, 4000, 3000, 4000};

// Returns COUNT for an unknown name
inline Animation animation_from_name(const char *name) {
  for (int i = 0; i < static_cast<int>(Animation::COUNT); i++)
    if (std::strcmp(name, ANIMATION_NAMES[i]) == 0)
      return static_cast<Animation>(i);
  return Animation::COUNT;
}

class AnimationPlayer {
 public:
  // duration_ms = 0: exactly one cycle; a longer duration repeats the animation.
  void start(Animation a, uint32_t duration_ms, uint32_t now_ms) {
    if (a == Animation::COUNT)
      return;
    animation_ = a;
    start_ms_ = now_ms;
    duration_ms_ = duration_ms > 0 ? duration_ms : ANIMATION_CYCLE_MS[static_cast<int>(a)];
    running_ = true;
  }
  void stop() { running_ = false; }
  bool active(uint32_t now_ms) const { return running_ && now_ms - start_ms_ < duration_ms_; }
  Animation animation() const { return animation_; }
  uint32_t elapsed_ms(uint32_t now_ms) const { return now_ms - start_ms_; }

 private:
  Animation animation_{Animation::DOOR};
  uint32_t start_ms_{0};
  uint32_t duration_ms_{0};
  bool running_{false};
};

inline void put(Frame &f, int col, int row, RGB c) {
  if (row >= 0 && row < ROWS && col >= 0 && col < COLS)
    f[led_index(row, col)] = c;
}

inline void hline(Frame &f, int row, int col0, int col1, RGB c) {
  for (int col = col0; col <= col1; col++)
    put(f, col, row, c);
}

inline void vline(Frame &f, int col, int row0, int row1, RGB c) {
  for (int row = row0; row <= row1; row++)
    put(f, col, row, c);
}

constexpr RGB ANIM_GREEN{0, 255, 60};

// Large check mark; pixels = how many of its points are drawn yet
inline void draw_check(Frame &f, RGB c, int pixels = 8) {
  static const uint8_t points[8][2] = {{2, 5}, {3, 6}, {4, 7}, {5, 6}, {6, 5}, {7, 4}, {8, 3}, {9, 2}};
  for (int i = 0; i < pixels && i < 8; i++)
    put(f, points[i][0], points[i][1], c);
}

// A door opens, warm light falls out, the door closes again
inline void draw_door(Frame &f, uint32_t t) {
  const RGB frame{220, 200, 170}, leaf{170, 80, 15}, handle{255, 220, 0}, glow{255, 190, 70};
  hline(f, 0, 2, 8, frame);
  vline(f, 2, 0, 9, frame);
  vline(f, 8, 0, 9, frame);
  int open = 0;  // 0..1000
  if (t < 500)
    open = 0;
  else if (t < 2000)
    open = (t - 500) * 1000 / 1500;
  else if (t < 3500)
    open = 1000;
  else if (t < 5000)
    open = (5000 - t) * 1000 / 1500;
  const int width = 5 - open * 4 / 1000;  // visible width of the door leaf, hinge on the left
  for (int row = 1; row < ROWS; row++)
    for (int col = 3; col <= 7; col++)
      put(f, col, row, col - 3 < width ? leaf : glow);
  if (width > 1)
    put(f, 3 + width - 1, 5, handle);
}

// Swinging bell with sound waves
inline void draw_bell(Frame &f, uint32_t t) {
  const RGB bell{255, 190, 0}, clapper{255, 70, 0}, wave{120, 120, 255};
  static const uint8_t half_width[] = {0, 1, 2, 2, 2, 3, 4};  // rows 1..7
  const float swing = sinf(TWO_PI_F * (t % 600) / 600.0f) * 1.6f;
  for (int i = 0; i < 7; i++) {
    const int row = i + 1;
    const int shift = static_cast<int>(floorf(swing * i / 6.0f + 0.5f));
    hline(f, row, 5 + shift - half_width[i], 5 + shift + half_width[i], bell);
  }
  put(f, 5 - static_cast<int>(floorf(swing + 0.5f)), 8, clapper);
  if (swing > 1.0f)
    vline(f, 10, 2, 5, wave);
  else if (swing < -1.0f)
    vline(f, 0, 2, 5, wave);
}

// A 3D printer prints a cube layer by layer, then a green check mark
inline void draw_printer(Frame &f, uint32_t t) {
  if (t >= 6000) {
    draw_check(f, ANIM_GREEN);
    return;
  }
  const RGB body{110, 110, 120}, plastic{255, 90, 0}, head{255, 255, 255};
  const int layer_ms = 1200, first = 3, width = 5;
  const int layer = t / layer_ms;
  const int progress = (t % layer_ms) * width / layer_ms;
  vline(f, 0, 0, 9, body);
  vline(f, 10, 0, 9, body);
  hline(f, 9, 1, 9, body);
  for (int l = 0; l < layer; l++)
    hline(f, 8 - l, first, first + width - 1, plastic);
  // Every other layer the print head travels the other way
  const int col = layer % 2 == 0 ? first + progress : first + width - 1 - progress;
  const int row = 8 - layer;
  if (layer % 2 == 0)
    hline(f, row, first, col, plastic);
  else
    hline(f, row, col, first + width - 1, plastic);
  hline(f, row - 2, 1, 9, body);
  put(f, col, row - 2, head);
  put(f, col, row - 1, head);
}

// The drum spins up, slows down again and finishes with a green check mark
inline void draw_washer(Frame &f, uint32_t t) {
  if (t >= 6000) {
    draw_check(f, ANIM_GREEN);
    return;
  }
  const RGB body{130, 130, 140}, drum{220, 220, 230}, water{0, 70, 255}, foam{255, 255, 255}, led{0, 255, 60};
  vline(f, 1, 0, 9, body);
  vline(f, 9, 0, 9, body);
  hline(f, 0, 1, 9, body);
  hline(f, 9, 1, 9, body);
  put(f, 3, 1, foam);
  if (t / 400 % 2 == 0)
    put(f, 7, 1, led);
  for (int row = 2; row <= 8; row++)
    for (int col = 2; col <= 8; col++) {
      const int dc = col - 5, dr = row - 5;
      const int d2 = dc * dc + dr * dr;
      if (d2 > 12)
        continue;
      put(f, col, row, d2 >= 7 ? drum : water);
    }
  // Speed rises until halfway and falls again; the angle is its integral (6 turns in total)
  const float x = t / 6000.0f;
  const float turns = x < 0.5f ? 12.0f * x * x : 6.0f - 12.0f * (1.0f - x) * (1.0f - x);
  const float angle = turns * TWO_PI_F;
  const int dx = static_cast<int>(floorf(sinf(angle) * 1.7f + 0.5f));
  const int dy = static_cast<int>(floorf(cosf(angle) * 1.7f + 0.5f));
  put(f, 5 + dx, 5 - dy, foam);
  put(f, 5 - dx, 5 + dy, foam);
}

// A letter flies in from the left
inline void draw_mail(Frame &f, uint32_t t) {
  const RGB paper{255, 255, 255}, flap{255, 180, 0};
  const int offset = t < 1000 ? static_cast<int>(t) * 11 / 1000 - 11 : 0;
  hline(f, 2, 1 + offset, 9 + offset, paper);
  hline(f, 7, 1 + offset, 9 + offset, paper);
  vline(f, 1 + offset, 2, 7, paper);
  vline(f, 9 + offset, 2, 7, paper);
  for (int i = 0; i <= 3; i++) {
    put(f, 2 + i + offset, 3 + i, flap);
    put(f, 8 - i + offset, 3 + i, flap);
  }
  if (t >= 1200 && t / 300 % 2 == 0)  // the letter has arrived: red notification dot
    put(f, 10, 1, RGB{255, 0, 0});
}

inline void draw_done(Frame &f, uint32_t t) { draw_check(f, ANIM_GREEN, static_cast<int>(t / 90)); }

// Warning triangle with a blinking exclamation mark
inline void draw_warning(Frame &f, uint32_t t) {
  const RGB red{255, 0, 0}, mark{255, 255, 255};
  for (int row = 1; row <= 8; row++) {
    const int half = (row - 1) * 5 / 7;
    put(f, 5 - half, row, red);
    put(f, 5 + half, row, red);
  }
  hline(f, 8, 0, 10, red);
  if (t / 500 % 2 == 0) {
    vline(f, 5, 3, 5, mark);
    put(f, 5, 7, mark);
  }
}

inline Frame render_animation(Animation a, uint32_t elapsed_ms, uint8_t level) {
  Frame f;
  f.fill(BLACK);
  const uint32_t t = elapsed_ms % ANIMATION_CYCLE_MS[static_cast<int>(a)];
  switch (a) {
    case Animation::DOOR:
      draw_door(f, t);
      break;
    case Animation::DOORBELL:
      draw_bell(f, t);
      break;
    case Animation::PRINTER:
      draw_printer(f, t);
      break;
    case Animation::WASHER:
      draw_washer(f, t);
      break;
    case Animation::MAIL:
      draw_mail(f, t);
      break;
    case Animation::DONE:
      draw_done(f, t);
      break;
    case Animation::WARNING:
      draw_warning(f, t);
      break;
    case Animation::COUNT:
      break;
  }
  if (level != 255)
    for (auto &c : f)
      c = scale(c, level);
  return f;
}

// Smooth transition: moves every color channel by at most max_step towards the target.
// Linear rather than proportional: values are before gamma correction, so a linear fade looks even.
// A proportional (halving) fade went dark after ~3 frames and looked like visible steps.
inline void fade_towards(Frame &shown, const Frame &target, uint8_t max_step) {
  auto step = [max_step](uint8_t from, uint8_t to) -> uint8_t {
    if (to > from)
      return to - from <= max_step ? to : static_cast<uint8_t>(from + max_step);
    return from - to <= max_step ? to : static_cast<uint8_t>(from - max_step);
  };
  for (int i = 0; i < NUM_LEDS; i++)
    shown[i] = {step(shown[i].r, target[i].r), step(shown[i].g, target[i].g),
                step(shown[i].b, target[i].b)};
}

// Step size for fade_towards so that a full-on to off transition takes fade_ms
constexpr uint8_t fade_step(uint32_t frame_ms, uint32_t fade_ms) {
  return fade_ms <= frame_ms ? 255 : static_cast<uint8_t>((255 * frame_ms + fade_ms - 1) / fade_ms);
}

// Limits the estimated current (about 20 mA per color channel at 255) to max_ma,
// so that "everything blinks" in white does not overload the power supply.
inline void limit_current(Frame &f, uint32_t max_ma) {
  uint32_t sum = 0;
  for (const auto &c : f)
    sum += c.r + c.g + c.b;
  const uint32_t ma = sum * 20 / 255;
  if (ma <= max_ma || ma == 0)
    return;
  const uint8_t level = static_cast<uint8_t>(255ULL * max_ma / ma);
  for (auto &c : f)
    c = scale(c, level);
}

inline bool is_night(int hour, int start, int end) {
  if (start == end)
    return false;
  if (start < end)
    return hour >= start && hour < end;
  return hour >= start || hour < end;
}

// State shared by the ESPHome lambdas
struct Runtime {
  Timer timer;
  AnimationPlayer animation;
  Frame shown{};
};

inline Runtime &runtime() {
  static Runtime rt;
  return rt;
}

}  // namespace wordclock
