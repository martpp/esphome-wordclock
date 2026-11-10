// Host tests for esphome/wordclock/wordclock.h
//   ./test/run_tests.sh            run the tests
//   ./test/run_tests.sh demo       preview the timer in the terminal
//   ./test/run_tests.sh export     dump frames as JSON for tools/make_gifs.py and tools/make_preview.py
#include "../esphome/wordclock/wordclock.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <string>
#include <vector>

using namespace wordclock;

static int failures = 0;
#define CHECK(cond)                                                     \
  do {                                                                  \
    if (!(cond)) {                                                      \
      std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);     \
      failures++;                                                       \
    }                                                                   \
  } while (0)

// The front as documented in wordclock.h (row 3 middle in lower case = WiFi status field)
static const char *FRONT[ROWS][COLS] = {
    {"E", "S", "K", "I", "S", "T", "A", "F", "Ü", "N", "F"},
    {"Z", "E", "H", "N", "Z", "W", "A", "N", "Z", "I", "G"},
    {"D", "R", "E", "I", "V", "I", "E", "R", "T", "E", "L"},
    {"V", "O", "R", "w", "i", "f", "i", "N", "A", "C", "H"},
    {"H", "A", "L", "B", "A", "E", "L", "F", "Ü", "N", "F"},
    {"E", "I", "N", "S", "X", "A", "M", "Z", "W", "E", "I"},
    {"D", "R", "E", "I", "A", "U", "J", "V", "I", "E", "R"},
    {"S", "E", "C", "H", "S", "N", "L", "A", "C", "H", "T"},
    {"S", "I", "E", "B", "E", "N", "Z", "W", "Ö", "L", "F"},
    {"Z", "E", "H", "N", "E", "U", "N", "K", "U", "H", "R"},
};

static std::set<int> word_leds(const Word &w) {
  std::set<int> s;
  for (int c = w.col; c < w.col + w.len; c++)
    s.insert(led_index(w.row, c));
  return s;
}

static std::set<int> idx(std::initializer_list<int> l) { return std::set<int>(l); }

// Reads the lit words in reading order ("ES IST FÜNF NACH ZWEI").
static std::string words(const Mask &m) {
  std::string out;
  for (int r = 0; r < ROWS; r++) {
    bool in_word = false;
    for (int c = 0; c < COLS; c++) {
      if (m[led_index(r, c)]) {
        if (!in_word && !out.empty())
          out += ' ';
        out += FRONT[r][c];
        in_word = true;
      } else {
        in_word = false;
      }
    }
  }
  int dots = 0;
  for (int i = 0; i < NUM_DOTS; i++)
    dots += m[FIRST_DOT + i];
  if (dots)
    out += " +" + std::to_string(dots);
  return out;
}

static int lit(const Frame &f, int from = 0, int to = NUM_LETTERS) {
  int n = 0;
  for (int i = from; i < to; i++)
    n += f[i] != BLACK;
  return n;
}

static void test_layout_matches_original_code() {
  // LED indices copied 1:1 from the original sketch (Wortuhr V3 by Mirc0)
  CHECK(word_leds(ES).size() + word_leds(IST).size() == 5);
  std::set<int> es_ist = word_leds(ES);
  for (int i : word_leds(IST))
    es_ist.insert(i);
  CHECK(es_ist == idx({0, 1, 3, 4, 5}));
  CHECK(word_leds(UHR) == idx({101, 100, 99}));
  CHECK(word_leds(FUENF_M) == idx({7, 8, 9, 10}));
  CHECK(word_leds(ZEHN_M) == idx({18, 19, 20, 21}));
  CHECK(word_leds(ZWANZIG) == idx({11, 12, 13, 14, 15, 16, 17}));
  CHECK(word_leds(VIERTEL) == idx({26, 27, 28, 29, 30, 31, 32}));
  CHECK(word_leds(VOR) == idx({41, 42, 43}));
  CHECK(word_leds(NACH) == idx({33, 34, 35, 36}));
  CHECK(word_leds(HALB) == idx({44, 45, 46, 47}));
  CHECK(word_leds(DREIVIERTEL) == idx({22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32}));
  CHECK(word_leds(DREI) == idx({66, 67, 68, 69}));
  CHECK(word_leds(VIER) == idx({73, 74, 75, 76}));
  CHECK(word_leds(FUENF_H) == idx({51, 52, 53, 54}));
  CHECK(word_leds(EINS) == idx({62, 63, 64, 65}));
  CHECK(word_leds(ZWEI) == idx({55, 56, 57, 58}));
  CHECK(word_leds(SECHS) == idx({83, 84, 85, 86, 87}));
  CHECK(word_leds(ACHT) == idx({77, 78, 79, 80}));
  CHECK(word_leds(SIEBEN) == idx({88, 89, 90, 91, 92, 93}));
  CHECK(word_leds(ZWOELF) == idx({94, 95, 96, 97, 98}));
  CHECK(word_leds(ZEHN_H) == idx({106, 107, 108, 109}));
  CHECK(word_leds(NEUN) == idx({103, 104, 105, 106}));
  CHECK(word_leds(ELF) == idx({49, 50, 51}));
  CHECK(word_leds(EIN) == idx({63, 64, 65}));
  CHECK(word_leds(WIFI) == idx({37, 38, 39, 40}));
}

static void test_time_words() {
  struct Case {
    int h, m;
    Dialect d;
    const char *expect;
  } cases[] = {
      {14, 9, Dialect::WEST, "ES IST FÜNF NACH ZWEI +4"},
      {0, 0, Dialect::WEST, "ES IST ZWÖLF UHR"},
      {13, 0, Dialect::WEST, "ES IST EIN UHR"},
      {1, 2, Dialect::WEST, "ES IST EIN UHR +2"},
      {12, 30, Dialect::WEST, "ES IST HALB EINS"},
      {12, 37, Dialect::WEST, "ES IST FÜNF NACH HALB EINS +2"},
      {9, 45, Dialect::WEST, "ES IST VIERTEL VOR ZEHN"},
      {9, 45, Dialect::EAST, "ES IST DREIVIERTEL ZEHN"},
      {9, 15, Dialect::WEST, "ES IST VIERTEL NACH NEUN"},
      {9, 15, Dialect::EAST, "ES IST VIERTEL ZEHN"},
      {10, 25, Dialect::WEST, "ES IST FÜNF VOR HALB ELF"},
      {11, 50, Dialect::WEST, "ES IST ZEHN VOR ZWÖLF"},
      {23, 55, Dialect::WEST, "ES IST FÜNF VOR ZWÖLF"},
      {6, 20, Dialect::WEST, "ES IST ZWANZIG NACH SECHS"},
      {6, 40, Dialect::WEST, "ES IST ZWANZIG VOR SIEBEN"},
      {7, 10, Dialect::WEST, "ES IST ZEHN NACH SIEBEN"},
      {8, 5, Dialect::WEST, "ES IST FÜNF NACH ACHT"},
      {11, 15, Dialect::EAST, "ES IST VIERTEL ZWÖLF"},
      {3, 33, Dialect::WEST, "ES IST HALB VIER +3"},
      {4, 0, Dialect::WEST, "ES IST VIER UHR"},
      {5, 0, Dialect::WEST, "ES IST FÜNF UHR"},
  };
  for (const auto &c : cases) {
    const std::string got = words(time_mask(c.h, c.m, c.d));
    if (got != c.expect) {
      std::printf("FAIL %02d:%02d: expected \"%s\", got \"%s\"\n", c.h, c.m, c.expect, got.c_str());
      failures++;
    }
  }
  // Every minute of the day yields ES IST plus an hour word and leaves >= 60 free letters
  for (int h = 0; h < 24; h++)
    for (int m = 0; m < 60; m++)
      for (Dialect d : {Dialect::WEST, Dialect::EAST}) {
        Mask mk = time_mask(h, m, d);
        int used = 0;
        for (int i = 0; i < NUM_LETTERS; i++)
          used += mk[i];
        CHECK(used >= 10);
        CHECK(NUM_LETTERS - used >= static_cast<int>(FINAL_PHASE_S));
      }
}

static Style style() {
  Style s;
  s.clock_color = {200, 200, 200};
  s.timer_color = {255, 0, 0};
  s.prelude_level = 80;
  return s;
}

static void test_timer_final_minute() {
  Timer t;
  const uint32_t t0 = 4000000000u;  // shortly before millis() wraps around
  t.start(60000, 60000, t0);
  const Mask tm = time_mask(14, 9, Dialect::WEST);
  int time_letters = 0;
  for (int i = 0; i < NUM_LETTERS; i++)
    time_letters += tm[i];

  for (int s = 60; s >= 1; s--) {
    // Halfway through a second: s - 0.5 s left -> s letters
    const uint32_t now = t0 + (60 - s) * 1000 + 500;
    t.update(now, 0);
    CHECK(t.state() == TimerState::RUNNING);
    Frame f = render(14, 9, 0, true, t, now, style());
    int red = 0;
    for (int i = 0; i < NUM_LETTERS; i++)
      red += f[i] == RGB{255, 0, 0};
    CHECK(red == s);
    CHECK(lit(f) == s + time_letters);
  }
  // 0 -> ringing: everything blinks, the time stays visible
  const uint32_t end = t0 + 60000;
  CHECK(t.update(end, 120000));
  CHECK(t.state() == TimerState::RINGING);
  Frame on = render(14, 9, 0, true, t, end + 100, style());
  Frame off = render(14, 9, 0, true, t, end + 600, style());
  CHECK(lit(on, 0, NUM_LEDS) == NUM_LEDS);
  CHECK(lit(off) == time_letters);
  CHECK(on[led_index(5, 7)] == (RGB{200, 200, 200}));  // ZWEI in the clock color
  // ring duration elapsed
  CHECK(!t.update(end + 119000, 120000));
  CHECK(t.update(end + 120000, 120000));
  CHECK(t.state() == TimerState::IDLE);
}

static void test_timer_prelude() {
  Timer t;
  t.start(10 * 60000, 10 * 60000, 0);
  const Mask tm = time_mask(14, 9, Dialect::WEST);
  int avail = 0;
  for (int i = 0; i < NUM_LETTERS; i++)
    avail += !tm[i];

  Frame f = render(14, 9, 0, true, t, 0, style());
  CHECK(lit(f) == NUM_LETTERS);  // at the start everything is lit
  CHECK(f[led_index(9, 0)] == (RGB{80, 0, 0}));  // dimmed
  // decreases monotonically down to 60
  int prev = avail;
  for (uint32_t now = 0; now <= 9 * 60000; now += 1000) {
    const int n = timer_letter_count(t.remaining_ms(now), t.total_ms(), avail);
    CHECK(n <= prev && n >= 60);
    prev = n;
  }
  CHECK(prev == 60);
  // A short timer (< 60 s) starts directly in the final phase
  Timer k;
  k.start(20000, 20000, 0);
  CHECK(timer_letter_count(k.remaining_ms(0), k.total_ms(), avail) == 20);
  // cancel
  t.cancel();
  CHECK(lit(render(14, 9, 0, true, t, 1000, style())) == NUM_LETTERS - avail);
}

static void test_helpers() {
  CHECK(is_night(23, 22, 6) && is_night(3, 22, 6) && !is_night(12, 22, 6));
  CHECK(is_night(9, 8, 22) && !is_night(22, 8, 22) && !is_night(5, 5, 5));
  Frame f;
  f.fill(RGB{255, 255, 255});
  limit_current(f, 3500);
  uint32_t sum = 0;
  for (auto &c : f)
    sum += c.r + c.g + c.b;
  CHECK(sum * 20 / 255 <= 3500);
  Frame a{}, b;
  b.fill(RGB{255, 100, 3});
  // 400 ms at 40 ms per frame: exactly 10 even steps from off to full
  const uint8_t st = fade_step(40, 400);
  CHECK(st == 26);
  for (int i = 0; i < 9; i++) {
    fade_towards(a, b, st);
    CHECK(a[0].r == 26 * (i + 1));
  }
  fade_towards(a, b, st);
  CHECK(a == b);
  fade_towards(a, b, st);  // stays at the target
  CHECK(a == b);
  Frame z{};
  for (int i = 0; i < 10; i++)
    fade_towards(a, z, st);
  CHECK(a == z);
  CHECK(fade_step(40, 0) == 255);
}

static void test_palette() {
  CHECK(palette_color(PALETTES[0], 0) == (RGB{255, 0, 0}));
  // smooth: no big jumps anywhere on the color wheel, including the wrap-around
  for (int i = 0; i < NUM_PALETTES; i++) {
    RGB prev = palette_color(PALETTES[i], 65535);
    for (int pos = 0; pos < 65536; pos += 64) {
      const RGB c = palette_color(PALETTES[i], static_cast<uint16_t>(pos));
      CHECK(std::abs(c.r - prev.r) <= 16 && std::abs(c.g - prev.g) <= 16 && std::abs(c.b - prev.b) <= 16);
      prev = c;
    }
  }
  // Every word gets its own hue, all letters of a word the same one
  Style s = style();
  s.palette = &PALETTES[0];
  Timer t;
  Frame f = render(14, 10, 0, true, t, 12345, s);
  CHECK(f[led_index(0, 0)] == f[led_index(0, 1)]);    // E S
  CHECK(f[led_index(0, 0)] != f[led_index(0, 3)]);    // ES / IST
  CHECK(f[led_index(5, 7)] == f[led_index(5, 10)]);   // Z W E I
  // and it changes over time
  CHECK(render(14, 10, 0, true, t, 42345, s)[led_index(0, 0)] != f[led_index(0, 0)]);
  Mask m = time_mask(14, 10, Dialect::WEST);
  Groups g{};
  CHECK(word_groups(m, g) == 5);  // ES IST ZEHN NACH ZWEI
}

static bool lit_at(const Frame &f, int row, int col) { return f[led_index(row, col)] != BLACK; }

static void test_analog() {
  Style s = style();
  s.display = Display::ANALOG;
  s.seconds_hand = false;
  Timer t;
  // 3:00 -> hour hand points right, minute hand up
  Frame f = render(3, 0, 0, true, t, 0, s);
  CHECK(f[led_index(0, 5)] == s.clock_color);
  CHECK(f[led_index(2, 5)] == s.clock_color);
  CHECK(f[led_index(5, 7)] == s.clock_color);
  CHECK(f[led_index(9, 5)] != s.clock_color);  // only the faint 6 o'clock mark
  // 6:30 -> both point down
  f = render(6, 30, 0, true, t, 0, s);
  CHECK(f[led_index(9, 5)] == s.clock_color);
  // 9:15: minute hand right, hour hand left and already slightly past the 9
  f = render(9, 15, 0, true, t, 0, s);
  CHECK(f[led_index(5, 9)] == s.clock_color && f[led_index(4, 3)] == s.clock_color);
  // Seconds hand: a dot runs once around the border and stays on it
  s.seconds_hand = true;
  for (int sec = 0; sec < 60; sec++) {
    Groups g{};
    analog_groups(12, 0, sec, true, g);
    int on_border = 0;
    for (int i = 0; i < NUM_LETTERS; i++)
      on_border += g[i] == 3 && is_border(i);
    CHECK(on_border == 1);
  }
  // Timer letters also work with clock hands
  Timer k;
  k.start(30000, 30000, 0);
  s.seconds_hand = false;
  f = render(3, 0, 0, true, k, 0, s);
  int red = 0;
  for (int i = 0; i < NUM_LETTERS; i++)
    red += f[i] == s.timer_color;
  CHECK(red == 30);
}

static void test_timer_circle() {
  Style s = style();
  Timer t;
  t.start(60000, 60000, 0);
  auto count = [&](uint32_t now) {
    Frame f = render(14, 10, 0, false, t, now, s);
    int n = 0;
    for (int i = 0; i < NUM_LETTERS; i++)
      n += f[i] != BLACK;
    return n;
  };
  s.timer_style = TimerStyle::RING;
  int border = 0;
  for (int i = 0; i < NUM_LETTERS; i++)
    border += is_border(i);
  CHECK(border == 38);
  CHECK(count(0) == border);
  CHECK(count(59990) <= 2);
  int prev = border;
  for (uint32_t now = 0; now < 60000; now += 1000) {
    const int n = count(now);
    CHECK(n <= prev);
    prev = n;
  }
  // Halfway: only the right half is lit (12 -> 6 o'clock clockwise)
  Frame half = render(14, 10, 0, false, t, 30000, s);
  CHECK(lit_at(half, 4, 10) && !lit_at(half, 4, 0));
  s.timer_style = TimerStyle::PIE;
  CHECK(count(0) == NUM_LETTERS);
  CHECK(count(30000) > 40 && count(30000) < 70);
  // Before the last minute: one dimmed turn over the total time, then a full turn at full brightness
  Timer l;
  l.start(300000, 300000, 0);
  Frame a = render(0, 0, 0, false, l, 0, s);
  CHECK(a[led_index(4, 2)] == (RGB{80, 0, 0}));
  Frame b = render(0, 0, 0, false, l, 240000, s);
  int full = 0;
  for (int i = 0; i < NUM_LETTERS; i++)
    full += b[i] == s.timer_color;
  CHECK(full == NUM_LETTERS);
}

static void test_animations() {
  CHECK(animation_from_name("door") == Animation::DOOR);
  CHECK(animation_from_name("warning") == Animation::WARNING);
  CHECK(animation_from_name("nonexistent") == Animation::COUNT);
  for (int a = 0; a < static_cast<int>(Animation::COUNT); a++) {
    const Animation an = static_cast<Animation>(a);
    Frame prev{};
    int changes = 0;
    for (uint32_t t = 0; t < ANIMATION_CYCLE_MS[a]; t += 40) {
      Frame f = render_animation(an, t, 255);
      // animations only draw on the letters
      for (int i = FIRST_DOT; i < NUM_LEDS; i++)
        CHECK(f[i] == BLACK);
      changes += f != prev;
      prev = f;
    }
    if (changes < 3) {
      std::printf("FAIL animation %s does not move\n", ANIMATION_NAMES[a]);
      failures++;
    }
    CHECK(lit(render_animation(an, ANIMATION_CYCLE_MS[a] - 1, 255)) > 0);
  }
  // Door: leaf when closed, light when open
  CHECK(render_animation(Animation::DOOR, 0, 255)[led_index(5, 6)] == (RGB{170, 80, 15}));
  CHECK(render_animation(Animation::DOOR, 2500, 255)[led_index(5, 6)] == (RGB{255, 190, 70}));
  // night mode dims animations too
  CHECK(render_animation(Animation::WARNING, 0, 128)[led_index(8, 0)] == (RGB{128, 0, 0}));
  // Player
  AnimationPlayer p;
  CHECK(!p.active(0));
  p.start(Animation::DOORBELL, 0, 4294960000u);  // shortly before millis() wraps around
  CHECK(p.active(4294960000u + 2999));
  CHECK(!p.active(4294960000u + 3000));
  p.start(Animation::COUNT, 0, 0);  // unknown: changes nothing
  CHECK(p.animation() == Animation::DOORBELL);
  p.start(Animation::MAIL, 10000, 100);
  CHECK(p.active(10000));
  p.stop();
  CHECK(!p.active(10000));
}

// ---- Terminal preview -------------------------------------------------------

// Output: time = [X], timer = *X*, off = " x " (also colored in a terminal)
static void print_frame(const Frame &f, const Mask &time) {
  for (int r = 0; r < ROWS; r++) {
    std::printf("   ");
    for (int c = 0; c < COLS; c++) {
      const int i = led_index(r, c);
      const RGB p = f[i];
      const char *l = FRONT[r][c];
      if (p == BLACK)
        std::printf("\033[38;5;238m %s \033[0m", l);
      else
        std::printf("\033[1;38;2;%d;%d;%dm%c%s%c\033[0m", p.r, p.g, p.b, time[i] ? '[' : '*', l,
                    time[i] ? ']' : '*');
    }
    std::printf("\n");
  }
  std::printf("   ");
  for (int i = 0; i < (COLS - NUM_DOTS) * 3 / 2; i++)  // center the dots below the letters
    std::printf(" ");
  for (int i = 0; i < NUM_DOTS; i++)
    std::printf(f[FIRST_DOT + i] != BLACK ? " o " : " . ");
  std::printf("\n\n");
}

static void demo() {
  Style s;
  s.timer_color = {255, 60, 0};
  Timer t;
  const int h = 14, m = 9;
  const Mask tm = time_mask(h, m, s.dialect);
  std::printf("14:09 without a timer:\n");
  print_frame(render(h, m, 0, true, t, 0, s), tm);
  t.start(5 * 60000, 5 * 60000, 0);
  for (uint32_t rem_s : {300u, 180u, 61u, 60u, 45u, 10u, 1u}) {
    const uint32_t now = (300 - rem_s) * 1000;
    std::printf("5 min timer, %u s left:\n", rem_s);
    print_frame(render(h, m, 0, true, t, now, s), tm);
  }
  t.update(300000, 0);
  std::printf("Timer finished (blink phase on):\n");
  print_frame(render(h, m, 0, true, t, 300100, s), tm);
  std::printf("Timer finished (blink phase off):\n");
  print_frame(render(h, m, 0, true, t, 300600, s), tm);
}

// JSON for tools/make_gifs.py and tools/make_preview.py: real frames from the real code
static void json_frame(const Frame &f, bool &first) {
  std::printf("%s\"", first ? "" : ",");
  first = false;
  for (const auto &c : f)
    std::printf("%02x%02x%02x", c.r, c.g, c.b);
  std::printf("\"");
}

static bool first_scene = true;

template <typename F>
static void json_scene(const char *name, int fps, uint32_t duration_ms, F frame) {
  std::printf("%s\"%s\":{\"fps\":%d,\"frames\":[", first_scene ? "" : ",", name, fps);
  first_scene = false;
  bool first = true;
  for (uint32_t t = 0; t < duration_ms; t += 1000 / fps)
    json_frame(frame(t), first);
  std::printf("]}\n");
}

static void export_scenes() {
  std::printf("{");
  Timer idle;
  Style w;
  json_scene("Word clock, single color", 1, 1000, [&](uint32_t t) { return render(14, 9, 0, true, idle, t, w); });
  static const char *pal_names[] = {"Rainbow", "Sunset", "Ocean", "Forest", "Fire", "Pastel", "Aurora"};
  for (int i = 0; i < NUM_PALETTES; i++) {
    Style s;
    s.palette = &PALETTES[i];
    s.palette_period_ms = 12000;  // faster than the default (60 s) for the preview
    std::string n = std::string("Color set ") + pal_names[i];
    json_scene(n.c_str(), 5, 12000, [&](uint32_t t) { return render(14, 10, 0, true, idle, t, s); });
  }
  Style a;
  a.display = Display::ANALOG;
  json_scene("Analog clock, 12 hours time-lapse", 8, 12000, [&](uint32_t t) {
    const int minutes = t * 720 / 12000;
    return render(minutes / 60, minutes % 60, (t / 100) % 60, true, idle, t, a);
  });
  Style ap = a;
  ap.palette = &PALETTES[0];
  ap.palette_period_ms = 12000;
  json_scene("Analog clock with color set", 5, 12000, [&](uint32_t t) { return render(10, 8, (t / 1000) % 60, true, idle, t, ap); });
  // Time-lapse of a 5 minute timer, then blinking
  for (TimerStyle ts : {TimerStyle::LETTERS, TimerStyle::RING, TimerStyle::PIE}) {
    for (Display d : {Display::WORDS, Display::ANALOG}) {
      Style s;
      s.timer_style = ts;
      s.display = d;
      s.seconds_hand = false;
      std::string n = std::string(ts == TimerStyle::LETTERS ? "Timer letters" : ts == TimerStyle::RING ? "Timer ring" : "Timer pie") +
                      (d == Display::WORDS ? ", word clock" : ", analog clock") + " (5 min time-lapse)";
      json_scene(n.c_str(), 10, 20000, [&](uint32_t t) {
        Timer k;
        k.start(300000, 300000, 0);
        // first 4 min in 6 s, last minute in 10 s, then 4 s of blinking
        const uint32_t now = t < 6000 ? t * 40 : t < 16000 ? 240000 + (t - 6000) * 6 : 300000 + (t - 16000);
        k.update(now < 300000 ? now : 300000, 0);
        return render(14, 9, 0, true, k, now, s);
      });
    }
  }
  static const char *anim_names[] = {"Door", "Doorbell", "3D printer", "Washer", "Mail", "Done", "Warning"};
  for (int i = 0; i < static_cast<int>(Animation::COUNT); i++) {
    std::string n = std::string("Animation ") + anim_names[i];
    json_scene(n.c_str(), 25, ANIMATION_CYCLE_MS[i], [&](uint32_t t) { return render_animation(static_cast<Animation>(i), t, 255); });
  }
  std::printf("}\n");
}

int main(int argc, char **argv) {
  if (argc > 1 && std::strcmp(argv[1], "export") == 0) {
    export_scenes();
    return 0;
  }
  if (argc > 1 && std::strcmp(argv[1], "demo") == 0) {
    demo();
    return 0;
  }
  test_layout_matches_original_code();
  test_time_words();
  test_timer_final_minute();
  test_timer_prelude();
  test_helpers();
  test_palette();
  test_analog();
  test_timer_circle();
  test_animations();
  if (failures) {
    std::printf("%d failures\n", failures);
    return 1;
  }
  std::printf("All tests passed\n");
  return 0;
}
