#include "FastLED.h"
#include <BridgeServer.h>
#include <BridgeClient.h>

FASTLED_USING_NAMESPACE

#define DATA_PIN    3
#define LED_TYPE    WS2812B
#define COLOR_ORDER RBG
#define NUM_LEDS    120
#define BRIGHTNESS         250
#define FRAMES_PER_SECOND  120
#define GRAVITY    -1
#define h0         2
#define NUM_BALLS  4

CRGB leds[NUM_LEDS];
BridgeServer server;
int every;
CRGB fl_color = CRGB::Blue;

float h[NUM_BALLS] ;                         // An array of heights
float vImpact0 = sqrt( -2 * GRAVITY * h0 );  // Impact velocity of the ball when it hits the ground if "dropped" from the top of the strip
float vImpact[NUM_BALLS] ;                   // As time goes on the impact velocity will change, so make an array to store those values
float tCycle[NUM_BALLS] ;                    // The time since the last time the ball struck the ground
int   pos[NUM_BALLS] ;                       // The integer position of the dot on the strip (LED index)
long  tLast[NUM_BALLS] ;                     // The clock time of the last ground strike
float COR[NUM_BALLS] ;

void setup() {
  delay(1000);
  FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(BRIGHTNESS);
  Bridge.begin();
  every = 1;

  for (int i = 0 ; i < NUM_BALLS ; i++) {    // Initialize variables
    tLast[i] = millis();
    h[i] = h0;
    pos[i] = 0;                              // Balls start on the ground
    vImpact[i] = vImpact0;                   // And "pop" up at vImpact0
    tCycle[i] = 0;
    COR[i] = 0.90 - float(i) / pow(NUM_BALLS, 2);
  }

  server.listenOnLocalhost();
  //server.noListenOnLocalhost();
  server.begin();
}

// List of patterns to cycle through.  Each is defined as a separate function below.
typedef void (*SimplePatternList[])();
SimplePatternList gPatterns = { lava, fire, twinkle, rainbowWithGlitter, confetti, sinelon, juggle, bpm, ease, bounce };

uint8_t gCurrentPatternNumber = 0; // Index number of which pattern is current
uint8_t gHue = 0; // rotating "base color" used by many of the patterns

// Gradient palette "lava_gp"
DEFINE_GRADIENT_PALETTE( lava_gp ) {
  0,   0,  0,  0,
  18,  0,  0, 46,
  113,  0,  0, 96,
  142,  3,  1, 108,
  175, 17,  1, 119,
  213, 44,  2, 146,
  255, 82,  4, 174,
  255, 115,  4, 188,
  255, 156,  4, 202,
  255, 203,  4, 218,
  255, 255,  4, 234,
  255, 255, 71, 244,
  255, 255, 255, 255
};
CRGBPalette16 lava_palette = lava_gp;

// Gradient palette "fire_gp"
DEFINE_GRADIENT_PALETTE( fire_gp ) {
  0,   1,  1,  0,
  32,  5,  0,  76,
  192, 24,  0, 146,
  220, 105,  5, 197,
  252, 255, 31, 240,
  252, 255, 111, 250,
  255, 255, 255, 255
};
CRGBPalette16 fire_palette = fire_gp;

void loop() {
  gPatterns[gCurrentPatternNumber]();

  // send the 'leds' array out to the actual LED strip
  FastLED.show();
  // insert a delay to keep the framerate modest
  FastLED.delay(1000 / FRAMES_PER_SECOND);
  BridgeClient client = server.accept();

  if (every == 1) {
    // do some periodic updates
    EVERY_N_MILLISECONDS( 20 ) {
      gHue++;
    }
    EVERY_N_SECONDS( 10 ) {
      nextPattern();
    }
  }

  process(client);
}

void process(BridgeClient client) {
  if (client.connected()) {
    int pin, ok;
    String color;
    ok = 0;
    String first_command = client.readStringUntil('/');
    first_command.trim();
    if (first_command == "animation") {
      ok = 1;
      every = 0;
      String animation = client.readString();
      animation.trim();
      // Get animation and color
      int slash = animation.indexOf('/');
      color = animation.substring(slash + 1);
      animation = animation.substring(0, slash);
      char char_animation = animation.charAt(0);
      if (color == "red") {
        fl_color = CRGB::Blue;
      } else if (color == "green") {
        fl_color = CRGB::Red;
      } else if (color == "blue") {
        fl_color = CRGB::Green;
      } else if (color == "purple") {
        fl_color = CRGB::Cyan;
      } else if (color == "white") {
        fl_color = CRGB::White;
      } else if (color == "yellow") {
        fl_color = CRGB::DarkMagenta;
      } else {
        color = "";
      }
      switch (char_animation) {
        case 'l':
          // lava
          gCurrentPatternNumber = 0;
          break;
        case 'f':
          // fire
          gCurrentPatternNumber = 1;
          break;
        case 't':
          // twinkle
          gCurrentPatternNumber = 2;
          break;
        case 'r':
          // rainbow
          gCurrentPatternNumber = 3;
          break;
        case 'c':
          // confetti
          gCurrentPatternNumber = 4;
          break;
        case 's':
          // sinelon
          gCurrentPatternNumber = 5;
          break;
        case 'j':
          // jiggle
          gCurrentPatternNumber = 6;
          break;
        case 'b':
          // bpm
          gCurrentPatternNumber = 7;
          break;
        case 'e':
          // ease
          gCurrentPatternNumber = 8;
          break;
        case 'q':
          // bounce
          gCurrentPatternNumber = 9;
          break;
        case 'n':
          // nextpattern
          nextPattern();
          break;
        case 'a':
          // auto
          every = 1;
          break;
      }
      client.println(animation);
      client.println(color);
    }
    client.stop();
  }
}

#define ARRAY_SIZE(A) (sizeof(A) / sizeof((A)[0]))

void nextPattern() {
  gCurrentPatternNumber = (gCurrentPatternNumber + 1) % ARRAY_SIZE(gPatterns);
}

// This function draws color waves with an ever-changing
void colorwaves( CRGB* ledarray, uint16_t numleds, CRGBPalette16& palette) {
  static uint16_t sPseudotime = 0;
  static uint16_t sLastMillis = 0;
  static uint16_t sHue16 = 0;

  uint8_t sat8 = beatsin88( 87, 220, 250);
  uint8_t brightdepth = beatsin88( 341, 96, 224);
  uint16_t brightnessthetainc16 = beatsin88( 203, (25 * 256), (40 * 256));
  uint8_t msmultiplier = beatsin88(147, 23, 60);

  uint16_t hue16 = sHue16;//gHue * 256;
  uint16_t hueinc16 = beatsin88(113, 300, 1500);

  uint16_t ms = millis();
  uint16_t deltams = ms - sLastMillis ;
  sLastMillis  = ms;
  sPseudotime += deltams * msmultiplier;
  sHue16 += deltams * beatsin88( 400, 5, 9);
  uint16_t brightnesstheta16 = sPseudotime;

  for ( uint16_t i = 0 ; i < numleds; i++) {
    hue16 += hueinc16;
    uint8_t hue8 = hue16 / 256;
    uint16_t h16_128 = hue16 >> 7;
    if ( h16_128 & 0x100) {
      hue8 = 255 - (h16_128 >> 1);
    } else {
      hue8 = h16_128 >> 1;
    }

    brightnesstheta16  += brightnessthetainc16;
    uint16_t b16 = sin16( brightnesstheta16  ) + 32768;

    uint16_t bri16 = (uint32_t)((uint32_t)b16 * (uint32_t)b16) / 65536;
    uint8_t bri8 = (uint32_t)(((uint32_t)bri16) * brightdepth) / 65536;
    bri8 += (255 - brightdepth);

    uint8_t index = hue8;
    index = scale8( index, 240);

    CRGB newcolor = ColorFromPalette( palette, index, bri8);

    uint16_t pixelnumber = i;
    pixelnumber = (numleds - 1) - pixelnumber;

    nblend( ledarray[pixelnumber], newcolor, 128);
  }
}

void rainbowWithGlitter() {
  fill_rainbow( leds, NUM_LEDS, gHue, 7);
  addGlitter(80);
}

void addGlitter( fract8 chanceOfGlitter) {
  if ( random8() < chanceOfGlitter) {
    leds[ random16(NUM_LEDS) ] += CRGB::White;
  }
}

void confetti() {
  fadeToBlackBy( leds, NUM_LEDS, 10);
  int pos = random16(NUM_LEDS);
  leds[pos] += CHSV( gHue + random8(64), 200, 255);
}

void sinelon() {
  fadeToBlackBy( leds, NUM_LEDS, 20);
  int pos = beatsin16( 13, 0, NUM_LEDS - 1 );
  leds[pos] += CHSV( gHue, 255, 192);
}

void bpm() {
  uint8_t BeatsPerMinute = 62;
  CRGBPalette16 palette = PartyColors_p;
  uint8_t beat = beatsin8( BeatsPerMinute, 64, 255);
  for ( int i = 0; i < NUM_LEDS; i++) { //9948
    leds[i] = ColorFromPalette(palette, gHue + (i * 2), beat - gHue + (i * 10));
  }
}

void juggle() {
  fadeToBlackBy( leds, NUM_LEDS, 20);
  byte dothue = 0;
  for ( int i = 0; i < 8; i++) {
    leds[beatsin16( i + 7, 0, NUM_LEDS - 1 )] |= CHSV(dothue, 200, 255);
    dothue += 32;
  }
}

void twinkle() {
  int i = random(NUM_LEDS);
  if (i < NUM_LEDS) leds[i] = CHSV(random(255), random(255), random(255));
  for (int j = 0; j < NUM_LEDS; j++) leds[j].fadeToBlackBy(8);

  FastLED.show();
  FastLED.delay(50);
}

void lava() {
  colorwaves( leds, NUM_LEDS, lava_palette);
}

void fire() {
  colorwaves( leds, NUM_LEDS, fire_palette);
}

void ease() {
  static uint8_t easeOutVal = 0;
  static uint8_t easeInVal  = 0;
  static uint8_t lerpVal    = 0;

  easeOutVal = ease8InOutQuad(easeInVal);
  easeInVal++;

  lerpVal = lerp8by8(0, NUM_LEDS, easeOutVal);

  leds[lerpVal] = fl_color;
  fadeToBlackBy(leds, NUM_LEDS, 250);
}

void bounce() {
  for (int i = 0 ; i < NUM_BALLS ; i++) {
    tCycle[i] =  millis() - tLast[i];
    h[i] = 0.5 * GRAVITY * pow( tCycle[i] / 1000 , 2.0 ) + vImpact[i] * tCycle[i] / 1000;

    if ( h[i] < 0 ) {
      h[i] = 0;  
      vImpact[i] = COR[i] * vImpact[i];
      tLast[i] = millis();

      if ( vImpact[i] < 0.01 ) vImpact[i] = vImpact0; 
    }
    pos[i] = round( h[i] * (NUM_LEDS - 1) / h0);  
  }

  for (int i = 0 ; i < NUM_BALLS ; i++) leds[pos[i]] = CHSV( uint8_t (i * 80) , 255, 255);
  FastLED.show();

  for (int i = 0 ; i < NUM_BALLS ; i++) {
    leds[pos[i]] = CRGB::Black;
  }
}

