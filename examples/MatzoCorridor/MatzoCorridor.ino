#include <Arduino.h>
#include <ESP8266WiFi.h>

// Experimental, non-playable corridor tour. Original map; no game assets.
// Same wiring as MatzoShapes: D1 -> RC filter -> X, D2 -> RC filter -> Y.
constexpr uint8_t PIN_X=5, PIN_Y=4;
constexpr int32_t UNIT=1024, TRIG_UNIT=16384;
constexpr uint8_t RAYS=33;
constexpr int32_t SCREEN_EDGE=900;
constexpr uint16_t POINT_US=80;
constexpr uint8_t MAX_CORNER_LINES=3;
constexpr uint32_t TURN_MS=1500;
constexpr uint32_t TOUR_MS=38000; // 10 + 6 + 10 + 6 seconds, plus four turns.
constexpr uint8_t MAP_WIDTH=16, MAP_HEIGHT=12;
uint32_t startTime;

struct Camera { int32_t x,y; uint8_t angle; };
struct Hit { int32_t height; uint8_t face; };
struct P2 { int32_t x,y; };
Hit hits[RAYS];
// atan(screen X), quantized to the 256-step angle convention.
// Rectilinear ray spacing keeps perspective wall edges approximately straight.
const int8_t rayOffsets[RAYS] PROGMEM={
  -32,-31,-29,-28,-26,-25,-23,-21,-19,-17,-15,-12,-10,-8,-5,-3,
  0,3,5,8,10,12,15,17,19,21,23,25,26,28,29,31,32
};

const int16_t sineTable[65] PROGMEM={
  0,402,804,1205,1606,2006,2404,2801,
  3196,3590,3981,4370,4756,5139,5520,5897,
  6270,6639,7005,7366,7723,8076,8423,8765,
  9102,9434,9760,10080,10394,10702,11003,11297,
  11585,11866,12140,12406,12665,12916,13160,13395,
  13623,13842,14053,14256,14449,14635,14811,14978,
  15137,15286,15426,15557,15679,15791,15893,15986,
  16069,16143,16207,16261,16305,16340,16364,16379,16384
};
int32_t sinQ(uint8_t a) {
  uint8_t quadrant=a>>6,index=a&63;
  if(quadrant&1) index=64-index;
  int32_t v=pgm_read_word(&sineTable[index]);
  return quadrant>=2 ? -v : v;
}
int32_t cosQ(uint8_t a) { return sinQ((uint8_t)(a+64)); }
int32_t magnitude(int32_t v) { return v<0 ? -v : v; }

// Four rooms connected by a rectangular corridor loop.
bool isWall(int32_t x,int32_t y) {
  if(x<=0 || y<=0 || x>=MAP_WIDTH-1 || y>=MAP_HEIGHT-1) return true;
  bool roomLeft=(x>=1 && x<=4);
  bool roomRight=(x>=10 && x<=14);
  bool roomTop=(y>=1 && y<=4);
  bool roomBottom=(y>=6 && y<=10);
  if((roomLeft || roomRight) && (roomTop || roomBottom)) return false;
  if((y==2 || y==8) && x>=2 && x<=12) return false;
  if((x==2 || x==12) && y>=2 && y<=8) return false;
  return true;
}

Camera cameraAt(uint32_t elapsed) {
  uint32_t t=elapsed%TOUR_MS;
  const int32_t left=2560,right=12800,top=2560,bottom=8704;
  Camera c={left,top,0};
  for(uint8_t leg=0;leg<4;leg++) {
    uint32_t travel=(leg%2==0) ? 10000 : 6000;
    int32_t x0=(leg==0 || leg==3) ? left : right;
    int32_t y0=(leg<2) ? top : bottom;
    int32_t x1=(leg==0 || leg==1) ? right : left;
    int32_t y1=(leg==0 || leg==3) ? top : bottom;
    if(t<travel) {
      c.x=x0+(x1-x0)*(int32_t)t/(int32_t)travel;
      c.y=y0+(y1-y0)*(int32_t)t/(int32_t)travel;
      c.angle=leg*64;
      return c;
    }
    t-=travel;
    if(t<TURN_MS) {
      c.x=x1; c.y=y1;
      c.angle=(uint8_t)(leg*64+t*64/TURN_MS);
      return c;
    }
    t-=TURN_MS;
  }
  return c;
}

Hit castRay(const Camera& c,int32_t offset) {
  constexpr int32_t STEP=64;
  constexpr uint16_t MAX_STEPS=224;
  uint8_t angle=(uint8_t)(c.angle+offset);
  int32_t dx=cosQ(angle)*STEP/TRIG_UNIT;
  int32_t dy=sinQ(angle)*STEP/TRIG_UNIT;
  int32_t x=c.x,y=c.y;
  int32_t previousX=x/UNIT;
  uint16_t distanceStep=MAX_STEPS;
  uint8_t face=255;
  for(uint16_t step=1;step<=MAX_STEPS;step++) {
    x+=dx; y+=dy;
    int32_t cellX=x/UNIT,cellY=y/UNIT;
    if(isWall(cellX,cellY)) {
      distanceStep=step;
      // Adjacent hits on the same wall plane should not create tile seams.
      face=(cellX!=previousX) ? (uint8_t)cellX : (uint8_t)(32+cellY);
      break;
    }
    previousX=cellX;
  }
  // Correct distance relative to camera direction to avoid a curved wall.
  int32_t depth=(int32_t)distanceStep*STEP*cosQ((uint8_t)offset)/TRIG_UNIT;
  if(depth<128) depth=128;
  int32_t height=500*UNIT/depth;
  if(height>SCREEN_EDGE) height=SCREEN_EDGE;
  return {height,face};
}

uint8_t pwmValue(int32_t coordinate) {
  int32_t v=128+coordinate*100/UNIT;
  if(v<0) v=0;
  if(v>255) v=255;
  return (uint8_t)v;
}
void point(P2 p) {
  analogWrite(PIN_X,pwmValue(p.x));
  analogWrite(PIN_Y,pwmValue(p.y));
}
void line(P2 a,P2 b) {
  int32_t dx=b.x-a.x,dy=b.y-a.y;
  int32_t length=magnitude(dx);
  if(magnitude(dy)>length) length=magnitude(dy);
  int32_t steps=(length+69)/70;
  if(steps<1) steps=1;
  if(steps>32) steps=32;
  for(int32_t i=1;i<=steps;i++) {
    point({a.x+dx*i/steps,a.y+dy*i/steps});
    delayMicroseconds(POINT_US);
  }
}
int32_t screenX(uint8_t ray) {
  return -SCREEN_EDGE+(2*SCREEN_EDGE*(int32_t)ray)/(RAYS-1);
}

void render(const Camera& c) {
  bool corners[RAYS]={false};
  for(uint8_t i=0;i<RAYS;i++) {
    int32_t offset=(int8_t)pgm_read_byte(&rayOffsets[i]);
    hits[i]=castRay(c,offset);
  }
  // Favor visible center corners and limit extra vertical strokes.
  uint8_t count=0;
  for(uint8_t radius=0;radius<RAYS/2 && count<MAX_CORNER_LINES;radius++) {
    for(uint8_t side=0;side<2 && count<MAX_CORNER_LINES;side++) {
      int32_t i=RAYS/2+(side==0 ? -(int32_t)radius : (int32_t)radius);
      if(i<=0 || i>=RAYS-1 || corners[i]) continue;
      if(hits[i].face!=hits[i-1].face &&
         magnitude(hits[i].height-hits[i-1].height)>20) {
        corners[i]=true; count++;
      }
    }
  }
  // One continuous circuit: top outline, right side, bottom, left side.
  // Vertical details are retraced back to the top, avoiding pen-up jumps.
  P2 previous={screenX(0),hits[0].height};
  point(previous);
  delayMicroseconds(POINT_US);
  for(uint8_t i=1;i<RAYS;i++) {
    P2 top={screenX(i),hits[i].height};
    line(previous,top);
    if(corners[i]) {
      P2 bottom={top.x,-top.y};
      line(top,bottom); line(bottom,top);
    }
    previous=top;
  }
  P2 bottom={screenX(RAYS-1),-hits[RAYS-1].height};
  line(previous,bottom); previous=bottom;
  for(int32_t i=RAYS-2;i>=0;i--) {
    bottom={screenX((uint8_t)i),-hits[i].height};
    line(previous,bottom); previous=bottom;
  }
  line(previous,{screenX(0),hits[0].height});
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_OFF);
  pinMode(PIN_X,OUTPUT); pinMode(PIN_Y,OUTPUT);
  analogWriteRange(255); analogWriteFreq(20000);
  point({0,0});
  startTime=millis();
  Serial.println();
  Serial.println("Matzo Corridor: automatic 38-second room and corridor tour");
}
void loop() {
  render(cameraAt(millis()-startTime));
  yield();
}
