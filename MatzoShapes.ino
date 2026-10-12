#include <Arduino.h>
#include <ESP8266WiFi.h>

// Clean, single-object show. No stars, text or disconnected balls.
// D1 -> 4.7k/100nF RC filter -> CH1/X.
// D2 -> separate 4.7k/100nF RC filter -> CH2/Y. Common GND.
constexpr uint8_t PIN_X=5, PIN_Y=4;
constexpr int32_t Q=1024, TQ=16384;
constexpr int32_t AMPLITUDE=100;
constexpr uint16_t POINT_US=80;
constexpr uint8_t MOTION_SPEED=2;  // 1=normal, 2=fast, 3=faster
constexpr uint32_t SCENE_MS=7000UL;
constexpr uint8_t SCENE_COUNT=8;
constexpr uint16_t CURVE_POINTS=128;
uint32_t startTime;
uint8_t previousScene=255;

struct P2 { int32_t x,y; };
struct P3 { int16_t x,y,z; };
struct Rotation { int32_t sx,cx,sy,cy,sz,cz; };

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
uint8_t pwmValue(int32_t coordinate) {
  int32_t v=128+coordinate*AMPLITUDE/Q;
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
  int32_t steps=(length+55)/56;
  if(steps<2) steps=2;
  if(steps>40) steps=40;
  for(int32_t i=0;i<=steps;i++) {
    point({a.x+dx*i/steps,a.y+dy*i/steps});
    delayMicroseconds(POINT_US);
  }
}
Rotation rotation(uint32_t t) {
  uint8_t ax=(uint8_t)(t/26),ay=(uint8_t)(t/19),az=(uint8_t)(t/73);
  return {sinQ(ax),cosQ(ax),sinQ(ay),cosQ(ay),sinQ(az),cosQ(az)};
}
Rotation shipRotation(uint32_t t) {
  // Keep the silhouette facing the viewer: bank and pitch instead of turning
  // fully edge-on. The limited angles keep cockpit details legible.
  uint8_t ax=(uint8_t)(12*sinQ((uint8_t)(t/19))/TQ);
  uint8_t ay=(uint8_t)(18*sinQ((uint8_t)(t/27+64))/TQ);
  uint8_t az=(uint8_t)(t/80);
  return {sinQ(ax),cosQ(ax),sinQ(ay),cosQ(ay),sinQ(az),cosQ(az)};
}
P2 project(P3 p,const Rotation& r) {
  int32_t y1=(p.y*r.cx-p.z*r.sx)/TQ;
  int32_t z1=(p.y*r.sx+p.z*r.cx)/TQ;
  int32_t x2=(p.x*r.cy+z1*r.sy)/TQ;
  int32_t z2=(-p.x*r.sy+z1*r.cy)/TQ;
  int32_t x3=(x2*r.cz-y1*r.sz)/TQ;
  int32_t y3=(x2*r.sz+y1*r.cz)/TQ;
  int32_t depth=3584+z2;
  return {x3*2867/depth,y3*2867/depth};
}
void solid(const P3* vertices,uint8_t count,const uint8_t* path,
           uint8_t pathCount,const Rotation& r) {
  P2 screen[16];
  for(uint8_t i=0;i<count;i++) screen[i]=project(vertices[i],r);
  for(uint8_t i=0;i+1<pathCount;i++) line(screen[path[i]],screen[path[i+1]]);
}

const P3 cube[]={
  {-563,-563,-563},{563,-563,-563},{-563,563,-563},{563,563,-563},
  {-563,-563,563},{563,-563,563},{-563,563,563},{563,563,563}
};
const uint8_t cubePath[]={0,1,3,2,0,4,5,1,5,7,3,7,6,2,6,4,0};
const P3 octa[]={
  {0,870,0},{870,0,0},{0,0,870},{-870,0,0},{0,0,-870},{0,-870,0}
};
const uint8_t octaPath[]={0,1,2,0,3,2,5,1,4,3,5,4,0};
const P3 tetra[]={
  {520,520,520},{520,-520,-520},{-520,520,-520},{-520,-520,520}
};
const uint8_t tetraPath[]={0,1,2,0,3,1,3,2,0};
const P3 pyramid[]={
  {-620,-450,-620},{620,-450,-620},{620,-450,620},
  {-620,-450,620},{0,850,0}
};
const uint8_t pyramidPath[]={0,1,2,3,0,4,2,3,4,1,0};

// Clean fighter outline with swept wings, twin rear notches and raised canopy.
// Deliberately fewer interior lines than the previous ship.
const P3 ship[]={
  {0,870,0},{180,300,0},{780,-340,0},{290,-220,0},
  {250,-650,0},{100,-470,0},{0,-720,0},{-100,-470,0},
  {-250,-650,0},{-290,-220,0},{-780,-340,0},{-180,300,0},
  {0,470,120},{-100,120,160},{0,-150,240},{100,120,160}
};
const uint8_t shipPath[]={
  0,1,2,3,4,5,6,7,8,9,10,11,0,
  12,13,14,15,12,14,12,0
};

// Precomputed, closed curves: circle, flower and a three-dimensional knot.
P3 curves[3][CURVE_POINTS];
P2 curveScreen[CURVE_POINTS];
void buildCurves() {
  for(uint16_t i=0;i<CURVE_POINTS;i++) {
    uint8_t a=(uint8_t)(i*256/CURVE_POINTS);
    int32_t c=cosQ(a),s=sinQ(a);
    curves[0][i]={(int16_t)(820*c/TQ),(int16_t)(820*s/TQ),0};
    int32_t radius=620+180*cosQ((uint8_t)(5*a))/TQ;
    curves[1][i]={(int16_t)(radius*c/TQ),(int16_t)(radius*s/TQ),0};
    radius=570+180*cosQ((uint8_t)(3*a))/TQ;
    curves[2][i]={
      (int16_t)(radius*cosQ((uint8_t)(2*a))/TQ),
      (int16_t)(radius*sinQ((uint8_t)(2*a))/TQ),
      (int16_t)(220*sinQ((uint8_t)(3*a))/TQ)
    };
  }
}
void curve(uint8_t which,uint32_t t) {
  if(which<2) {
    // Preserve the circle and flower outlines; only rotate in the screen plane.
    uint8_t a=(uint8_t)(t/31);
    int32_t c=cosQ(a),s=sinQ(a);
    int32_t size=930+80*sinQ((uint8_t)(t/21))/TQ;
    for(uint16_t i=0;i<CURVE_POINTS;i++) {
      int32_t x=curves[which][i].x,y=curves[which][i].y;
      curveScreen[i]={
        ((x*c-y*s)/TQ)*size/Q,
        ((x*s+y*c)/TQ)*size/Q
      };
    }
  } else {
    Rotation r=rotation(t);
    for(uint16_t i=0;i<CURVE_POINTS;i++)
      curveScreen[i]=project(curves[which][i],r);
  }
  for(uint16_t i=0;i<=CURVE_POINTS;i++) {
    point(curveScreen[i%CURVE_POINTS]);
    delayMicroseconds(POINT_US);
  }
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_OFF);
  pinMode(PIN_X,OUTPUT);
  pinMode(PIN_Y,OUTPUT);
  analogWriteRange(255);
  analogWriteFreq(20000);
  point({0,0});
  buildCurves();
  startTime=millis();
  Serial.println();
  Serial.println("Matzo Shapes: clean closed paths, no stars or text");
}
void loop() {
  uint32_t elapsed=millis()-startTime;
  uint8_t scene=(elapsed/SCENE_MS)%SCENE_COUNT;
  // Increase animation speed, not the speed of tracing each line.
  uint32_t t=elapsed*MOTION_SPEED;
  if(scene!=previousScene) {
    Serial.print("Shape: ");
    Serial.println(scene);
    previousScene=scene;
  }
  switch(scene) {
    case 0: solid(cube,8,cubePath,sizeof(cubePath),rotation(t)); break;
    case 1: curve(0,t); break;
    case 2: solid(octa,6,octaPath,sizeof(octaPath),rotation(t)); break;
    case 3: solid(tetra,4,tetraPath,sizeof(tetraPath),rotation(t)); break;
    case 4: solid(pyramid,5,pyramidPath,sizeof(pyramidPath),rotation(t)); break;
    case 5: curve(1,t); break;
    case 6: solid(ship,16,shipPath,sizeof(shipPath),shipRotation(t)); break;
    case 7: curve(2,t); break;
  }
  yield();
}
