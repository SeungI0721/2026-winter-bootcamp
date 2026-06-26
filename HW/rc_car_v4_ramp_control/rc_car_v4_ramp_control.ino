#include <Adafruit_NeoPixel.h>
#include <SoftwareSerial.h>

#define LDIR 12
#define LPWM 10
#define RDIR 13
#define RPWM 11
#define LEDP 5
#define BUZ 4
#define TRIG 7
#define ECHO 8
#define IRL A1
#define IRR A2

#define IR_TH 500
#define IR_HIT(v) ((v)<IR_TH)

#define DRIVE 120
#define DETECT 15
#define EMERG 6
#define LOCK_MS 2000UL
#define REV_MS 800UL
#define REV_SPD 80
#define PAUSE_MS 100UL
#define CLEAR 12
#define A_PAUSE 100UL
#define A_REV 300UL
#define A_FWD 500UL
#define SPIN 140
#define MAX_V 250   // 수동 램프 최대 속도(0~255)

#define SONAR_MS 70UL
#define CMD_SZ 8
#define MANUAL_TO 600UL

SoftwareSerial bt(2,3);
Adafruit_NeoPixel px(3,LEDP,NEO_GRB+NEO_KHZ800);

enum{IDLE,AUTO,MANUAL} mode=IDLE;
enum{NORM,LOCK,REC_REV,REC_PAUSE,AV_PAUSE,AV_REV,AV_FWD} st=NORM;

char buf[CMD_SZ]; byte bi=0;
long dist=999; unsigned long sonarAt=0,until=0,lastCmd=0;

int mspeed=160,cl=0,cr=0,tl=0,tr=0;
const int ACC=6,DEC_STEP=10; const unsigned long RAMP_MS=20; unsigned long rampAt=0;
int adir=-1;

static inline int cap(int v){v=abs(v);return v>255?255:v;}
void tone2(int f,int d){long p=1000000L/f,h=p/2;for(long i=0;i<d*1000L;i+=p){digitalWrite(BUZ,1);delayMicroseconds(h);digitalWrite(BUZ,0);delayMicroseconds(h);}}
void pxAll(byte r,byte g,byte b){for(byte i=0;i<3;i++)px.setPixelColor(i,px.Color(r,g,b));px.show();}

void mStop(){analogWrite(LPWM,0);analogWrite(RPWM,0);}
void mL(int p){digitalWrite(LDIR,p>=0);analogWrite(LPWM,cap(p));}
void mR(int p){digitalWrite(RDIR,p>=0);analogWrite(RPWM,cap(p));}
void fwd(int p){mL(p);mR(p);}
void rev(int p){mL(-p);mR(-p);}

long readCm(){
  digitalWrite(TRIG,0);delayMicroseconds(2);
  digitalWrite(TRIG,1);delayMicroseconds(10);
  digitalWrite(TRIG,0);
  unsigned long d=pulseIn(ECHO,1,18000UL);
  return d? (long)(d/58):999;
}
void sonar(unsigned long now){if((long)(now-sonarAt)<0)return;dist=readCm();sonarAt=now+SONAR_MS;}

int stepTo(int c,int t){
  if(c==t)return c;
  bool dc=((c>0&&t<0)||(c<0&&t>0));
  int s=dc?DEC_STEP:((abs(t)>abs(c))?ACC:DEC_STEP);
  if(c<t){c+=s;if(c>t)c=t;}else{c-=s;if(c<t)c=t;}
  return c;
}
void ramp(){
  unsigned long now=millis();
  if(now-rampAt<RAMP_MS)return;
  rampAt=now;
  cl=stepTo(cl,tl); cr=stepTo(cr,tr);
  mL(cl); mR(cr);
}

void setSt(byte s){
  st=s;
  switch(s){
    case NORM: break;
    case LOCK: mStop(); pxAll(255,0,0); break;
    case REC_REV: pxAll(255,0,0); rev(REV_SPD); break;
    case REC_PAUSE: mStop(); pxAll(255,0,0); break;
    case AV_PAUSE: mStop(); pxAll(255,255,0); break;
    case AV_REV: pxAll(255,255,0); rev(REV_SPD); break;
    case AV_FWD:
      if(adir<0){mL(-SPIN);mR(SPIN);}else{mL(SPIN);mR(-SPIN);}
      break;
  }
}
void emergency(unsigned long now){
  setSt(LOCK); tone2(440,70); until=now+LOCK_MS;
  tl=tr=0;
}
void avoidDir(unsigned long now,int d){adir=d<0?-1:1;setSt(AV_PAUSE);until=now+A_PAUSE;}
void avoidRnd(unsigned long now){adir=(random(0,2)?1:-1);setSt(AV_PAUSE);until=now+A_PAUSE;}

void cmdExec(){
  if(!bi)return;
  if(bi==1){
    char c=buf[0];
    mode=MANUAL; pxAll(0,0,255); lastCmd=millis();

    int v=mspeed; if(v>MAX_V)v=MAX_V;

    if(c=='F'){tl=+v;tr=+v;}
    else if(c=='B'){tl=-v;tr=-v;}
    else if(c=='L'){tl=-v;tr=+v;}
    else if(c=='R'){tl=+v;tr=-v;}
    else if(c=='S'){tl=0;tr=0;}
    return;
  }
  if(bi==5 && buf[0]=='s'&&buf[1]=='t'&&buf[2]=='a'&&buf[3]=='r'&&buf[4]=='t'){
    mode=AUTO; setSt(NORM); sonarAt=0; pxAll(0,255,0);
    return;
  }
  if(bi==6 && buf[0]=='m'&&buf[1]=='a'&&buf[2]=='n'&&buf[3]=='u'&&buf[4]=='a'&&buf[5]=='l'){
    mode=MANUAL; setSt(NORM); tl=tr=0; lastCmd=millis(); pxAll(0,0,255);
    return;
  }
}

void cmdChar(char ch){
  if(ch=='\r')return;
  if(ch=='_'||ch=='\n'){cmdExec(); bi=0; return;}
  if(bi<CMD_SZ-1)buf[bi++]=ch; else bi=0;
}

void setup(){
  Serial.begin(9600); bt.begin(9600);
  pinMode(LDIR,1);pinMode(LPWM,1);pinMode(RDIR,1);pinMode(RPWM,1);
  pinMode(BUZ,1); pinMode(TRIG,1); pinMode(ECHO,0);
  pinMode(IRL,0); pinMode(IRR,0);
  randomSeed(analogRead(A0));
  px.begin(); px.setBrightness(200); px.show();
  mStop(); tone2(659,80); tone2(784,80);
}

void loop(){
  while(Serial.available())cmdChar((char)Serial.read());
  while(bt.available())cmdChar((char)bt.read());

  unsigned long now=millis();
  sonar(now);

  if(st!=NORM){
    if(now>=until){
      if(st==LOCK){setSt(REC_REV); until=now+REV_MS;}
      else if(st==REC_REV){setSt(REC_PAUSE); until=now+PAUSE_MS;}
      else if(st==REC_PAUSE){dist=readCm(); if(dist<=CLEAR){setSt(REC_REV); until=now+400UL;} else setSt(NORM);}
      else if(st==AV_PAUSE){setSt(AV_REV); until=now+A_REV;}
      else if(st==AV_REV){setSt(AV_FWD); until=now+A_FWD;}
      else setSt(NORM);
    }
    return;
  }

  if(dist<=EMERG){emergency(now);return;}
  if(mode==IDLE){mStop();return;}

  if(mode==MANUAL){
    if(now-lastCmd>MANUAL_TO){tl=tr=0;}
    ramp();
    return;
  }

  if(mode==AUTO){
    int l=analogRead(IRL),r=analogRead(IRR);
    bool hl=IR_HIT(l),hr=IR_HIT(r);
    if(hl&&!hr){avoidDir(now,1);return;}
    if(!hl&&hr){avoidDir(now,-1);return;}
    if(hl&&hr){avoidRnd(now);return;}
    fwd(DRIVE);
    if(dist<=DETECT)avoidRnd(now);
  }
}
