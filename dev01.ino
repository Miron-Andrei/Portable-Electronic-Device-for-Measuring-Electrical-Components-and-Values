#include   <Adafruit_GFX.h>
#include   <Adafruit_ST7789.h> // Bibliotecă pentru ecrane TFT cu driver ST7789
#include   <SPI.h> // Bibliotecă pentru comunicația SPI
#include   <BH1750.h> // Bibliotecă pentru senzorul de lumină BH1750
#include   <Wire.h>  // Bibliotecă pentru comunicația I2C
BH1750 lightMeter;
// Pini pentru afișajul TFT
#define TFT_CS   10   // Chip select (CS)
#define TFT_RST   8   // Reset pin
#define TFT_DC    9   // Data/Command pin
// Stări buton (folosind rezistență de pull-up internă)
#define  PRESSED  LOW // Butonul este apăsat (conectat la GND)
#define RELEASED HIGH // Butonul este eliberat
///////// Culori în format RGB565 ///////////  
#define NEGRU    0x0000   // 0 - Negru
#define MARO    0xA145         // 1 - Maro
#define ROSU      0xf800     // 2 - Roșu
#define PORTOCALIU   0xFD20         // 3 - Portocaliu
#define GALBEN   0xffe0  // 4 - Galben
#define VERDE    0x0400   // 5 - Verde
#define ALBASTRU     0x001f    // 6 - Albastru
#define VIOLET   0xf81f         // 7 - Violet
#define GRI     0x8410   // 8 - Gri
#define ALB    0xffff   // 9 - Alb
#define REZISTOR 0xf735    // Albastru deschis
const int batteryPin = A6;   // Pin analogic pentru citirea tensiunii bateriei
const float R1_ref = 10000.0;   // Rezistență de referință (pentru divizor tensiune) - 10k Ohm
const float R2_ref = 10000.0;   // A doua rezistență de referință - 10k Ohm
// Configurare pini pentru măsurarea capacității
const int OUT_PIN = A2; // Pin de ieșire pentru măsurare capacitate
const int IN_PIN = A1; // Pin de intrare pentru măsurare capacitate
const float IN_STRAY_CAP_TO_GND = 24.48;  // Capacitate parazită către GND (determinată experimental)
const float IN_CAP_TO_GND  = IN_STRAY_CAP_TO_GND; // Capacitate efectivă către GND
const float R_PULLUP = 34.8;   // Rezistență de pull-up (Ohmi)
const int MAX_ADC_VALUE = 1023; // Valoarea maximă returnată de ADC pe 10 biți
// Pini pentru butoanele de control meniu
int   backButtonPin = 5;    // Buton Back (BB)
int     upButtonPin = 2;    // Buton Up (BU)
int   downButtonPin = 3;    // Buton Down (BD)
int selectButtonPin = 4;    // Buton Select (BS)
enum button_t : uint8_t {NWNE, UP, DOWN, SELECT, BACK} button; // Enumerare pentru stările butoanelor
uint16_t menuLineNumber[] = {0, 30, 80, 130, 180};  // Coordonatele liniilor de meniu pe ecran
// Variabile pentru ohmmetru
int analogPin = 0;  // Pin analogic pentru citirea tensiunii pe rezistență
int raw = 0;        // Valoare brută ADC
int Vin = 5;        // Tensiune de alimentare (Vcc)
float Vout = 0;     // Tensiune măsurată pe rezistență necunoscută
float R1 = 1000.0;  // Rezistență cunoscută (pentru divizor de tensiune)
float R2 = 0;       // Rezistență necunoscută
float buffer = 0;   // Variabilă temporară pentru calcule
// Pointer către funcția de măsurare selectată
void (*measurePtr)(void) = NULL;
int menuLine = 1;  // Linia curentă din meniu (începe de la 1)
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST); // Controlul ecranului TFT
void afisaremeniu() {
  tft.fillScreen(NEGRU);
  tft.setCursor(30, 30);
  tft.print("Rezistenta");
  tft.setCursor(30, 80);
  tft.print("Capacitate");
  tft.setCursor(30, 130);
  tft.print("Intensitatea Luminoasa");
  tft.setCursor(30, 180);
  tft.print("Informatii"); }
void afisareLine() {
  static uint16_t oldMenuLine = 0;
  tft.setTextColor(NEGRU); 
  tft.setCursor(10, menuLineNumber[oldMenuLine]);
  tft.print('>');
  tft.setTextColor(ALB); 
  oldMenuLine = menuLine;
  tft.setCursor(10, menuLineNumber[menuLine]);
  tft.print('>');}

void setup() { 
  pinMode(OUT_PIN, OUTPUT);  // Setează pinul OUT ca ieșire (pentru condensator)
  pinMode(IN_PIN, OUTPUT);   // Setează pinul IN ca ieșire (va fi modificat ulterior)
   // Configurarea butoanelor ca intrări cu pull-up intern activat
  pinMode(    upButtonPin, INPUT_PULLUP);  // Setare buton Up ca INPUT_PULLUP
  pinMode(  downButtonPin, INPUT_PULLUP);  // Setare buton Down ca INPUT_PULLUP
  pinMode(selectButtonPin, INPUT_PULLUP);  // Setare buton Select ca INPUT_PULLUP
  pinMode(  backButtonPin, INPUT_PULLUP);  // Setare buton Back ca INPUT_PULLUP
  Wire.begin();   // Inițializează comunicația I2C
  lightMeter.begin();  // Pornește senzorul de lumină BH1750
  tft.init(240, 320);  // Initializare ecran 240x320 pixeli
  tft.setRotation(3);  // Rotire ecran
  tft.setTextColor(ALB);  // Text alb
  tft.setTextSize(2);  // Dimensiune text
  afisaremeniu();  // Afiseaza meniul initial
  afisareLine(); } // Subliniază linia curentă din meniuv
void loop() {
  static button_t oldButton = NWNE;  // Memorează ultima stare a butonului
    // Verifică ce buton este apăsat
  if     (digitalRead(    upButtonPin) == RELEASED) { button = UP;}
  else if(digitalRead(  downButtonPin) == RELEASED) { button = DOWN;}
  else if(digitalRead(selectButtonPin) == RELEASED) { button = SELECT;}
  else if(digitalRead(  backButtonPin) == RELEASED) { button = BACK;}
  else { button = NWNE;}
  // Dacă s-a schimbat butonul față de ultimul apăsat
  if (oldButton != button) {
    oldButton = button;
    switch (button) {
      case UP:  // Navigare în meniu folosind butonul Up (BU)
        if (menuLine == 1) {
          menuLine = 4; }  // Revin la ultimul meniu daca scade sub 1
        else {
          menuLine--;  }  // Mergem la meniul anterior
        afisareLine();  // Actualizeaza meniul pe ecran
        break;
      case DOWN:  // Navigare în meniu folosind butonul Down (BD)
        if (menuLine == 4) {
          menuLine = 1;  } // Revin la primul meniu daca depaseste 4
        else {   menuLine++;  }   // Trecem la urmatorul meniu
        afisareLine();    // Actualizeaza meniul pe ecran
        break;
      case SELECT:  // Executa actiunea selectata cu butonul Select (BS)
        actiune();  // Executa actiunea pentru meniul curent    // bs_apasat = true;
        break;
      case BACK:  // Back
        measurePtr = NULL;
        afisaremeniu();
        afisareLine();  // Actualizeaza meniul pe ecran
        break;  }  }     // Dacă este setată o funcție de măsurare, o executăm
  if (measurePtr) (*measurePtr)();
  millis();  }

void actiune() {
  switch (menuLine) {
    case 1:
      tft.fillScreen(NEGRU);
      tft.setCursor(10, 30);
      tft.print("Masurare Rezistenta:");
      measurePtr = masurareRezistenta;
      break;
    case 2:
      tft.fillScreen(NEGRU);
      tft.setCursor(10, 30);
      tft.print("Masurare Capacitate:");
      measurePtr = masurareCapacitate;
      break;
    case 3:
      tft.fillScreen(NEGRU);
      tft.setCursor(10, 30);
      tft.print("Masurare Iluminarii:");
      measurePtr = masurareLumina;
      break;
    case 4:
       tft.fillScreen(NEGRU);
      measurePtr = Informatii;
      break;}
}

void masurareRezistenta() {
    raw = analogRead(analogPin);
    if(raw){
    buffer = raw * Vin;
    Vout = (buffer)/1024.0;
    buffer = (Vin/Vout) - 1;
    R2= (R1 * buffer);
  tft.fillRect(10, 50, 270, 30, NEGRU);  // Curata zona de afisare
  tft.setCursor(10, 50);
  if (R2 > 7000.0) {  
    tft.print("Rezistenta Necunoscuta");  }
  else if (R2 > 999.0) {  
    tft.print(R2 / 1000.0, 2);  
    tft.print(" kOhm");    }
  else {  // Rezisten?a în Ohmi
    tft.print(R2, 2);  // 2 zecimale pentru precizie
    tft.print(" Ohm");   }
   deseneazaRezistenta();  }
  delay(200);   }

void masurareCapacitate() {
    tft.fillRect(10, 50, 220, 30, NEGRU); 
    tft.setCursor(10, 50);
    deseneazaCondensator();
    pinMode(IN_PIN, INPUT);
    digitalWrite(OUT_PIN, HIGH);
    int val = analogRead(IN_PIN);
    digitalWrite(OUT_PIN, LOW);
    if (val < 1000){
      pinMode(IN_PIN, OUTPUT);
      float capacitance =  (float)val * IN_CAP_TO_GND / (float)(MAX_ADC_VALUE - val) ;
      tft.print(capacitance, 3);
      tft.print(F(" pF "));    }
    else{
      pinMode(IN_PIN, OUTPUT);
      delay(1);
      pinMode(OUT_PIN, INPUT_PULLUP);
      unsigned long u1 = micros();
      unsigned long t;
      int digVal;
      do{
        digVal = digitalRead(OUT_PIN);
        unsigned long u2 = micros();
        t = u2 > u1 ? u2 - u1 : u1 - u2;
      } while ((digVal < 1) && (t < 400000L));
      pinMode(OUT_PIN, INPUT);  
      val = analogRead(OUT_PIN);
      digitalWrite(IN_PIN, HIGH);
      int dischargeTime = (int)(t / 1000L) * 5;
      delay(dischargeTime);   
      pinMode(OUT_PIN, OUTPUT);  
      digitalWrite(OUT_PIN, LOW);
      digitalWrite(IN_PIN, LOW);
      float capacitance = -(float)t / R_PULLUP / log(1.0 - (float)val / (float)MAX_ADC_VALUE ) ;
      if (capacitance  > 1000.0){
        tft.print(capacitance / 1000.0, 2);
        tft.print(F(" uF"));  }
      else{
        tft.print(capacitance, 2);
        tft.print(F(" nF"));      }     }
    while (millis() % 1000 != 0); 
    if (digitalRead(backButtonPin) == RELEASED) {
    measurePtr = NULL;
    afisaremeniu();
    afisareLine();
    return;   }    }

void masurareLumina() { 
  float lux = lightMeter.readLightLevel();
   tft.fillRect(10, 50, 220, 30, NEGRU);  // Curata zona de afisare
   tft.setCursor(10, 50);
   tft.print(lux);
   tft.print(" lx");
   delay(100);
   deseneazaSoare(lux);     }

void deseneazaRezistenta() {
  tft.fillRect(30, 135, 40,10, GRI);   // stânga
  tft.fillRect(170, 135, 40,10, GRI);  // dreapta
  tft.fillRoundRect(70, 120, 100, 40, 5, REZISTOR);  // Corpul rezistenței
  if (R2 >= 90   && R2 <140)                  // 100 
  { tft.fillRect(80, 120, 8, 40, MARO);  // Banda 1
    tft.fillRect(100, 120, 8, 40, NEGRU);  // Banda 2
    tft.fillRect(120, 120, 8, 40, MARO); // Banda 3 
  } 
  else if (R2 >= 200 && R2 < 270)            // 220
  { tft.fillRect(80, 120, 8, 40, ROSU);  // Banda 1
    tft.fillRect(100, 120, 8, 40, ROSU);  // Banda 2
    tft.fillRect(120, 120, 8, 40, MARO); // Banda 3
  } 
  else if (R2 >= 300 && R2 < 370)            // 330 
  { tft.fillRect(80, 120, 8, 40, PORTOCALIU);
    tft.fillRect(100, 120, 8, 40, PORTOCALIU);
    tft.fillRect(120, 120, 8, 40, MARO);  // Banda 1
  } 
  else if (R2 >= 430 && R2 < 520)            // 470
  { tft.fillRect(80, 120, 8, 40, GALBEN);  // Banda 1
    tft.fillRect(100, 120, 8, 40, VIOLET);  // Banda 2
    tft.fillRect(120, 120, 8, 40, MARO); // Banda 3
  } 
  else if (R2 >= 650 && R2 < 740)            // 680
  { tft.fillRect(80, 120, 8, 40, ALBASTRU);  // Banda 1
    tft.fillRect(100, 120, 8, 40, GRI);  // Banda 2
    tft.fillRect(120, 120, 8, 40, MARO); // Banda 3
  } 
  else if (R2 >= 950 && R2 < 1030)            // 1 kOhm
  { tft.fillRect(80, 120, 8, 40, MARO); // Banda 1
    tft.fillRect(100, 120, 8, 40, NEGRU);  // Banda 2
    tft.fillRect(120, 120, 8, 40, ROSU); // Banda 3
  } 
  else if (R2 >= 1100 && R2 < 1250)            // 1.2 KOhm
  { tft.fillRect(80, 120, 8, 40, MARO); // Banda 1
    tft.fillRect(100, 120, 8, 40, ROSU);  // Banda 2
    tft.fillRect(120, 120, 8, 40, ROSU); // Banda 3
  } 
  else if (R2 >= 1400 && R2 < 1600)            // 1.5 kOhm
  { tft.fillRect(80, 120, 8, 40, MARO); // Banda 1
    tft.fillRect(100, 120, 8, 40, VERDE);  // Banda 2
    tft.fillRect(120, 120, 8, 40, ROSU); // Banda 3

  } 
  else if (R2 >= 2100 && R2 < 2300)            // 2.2 kOhm
  { tft.fillRect(80, 120, 8, 40, ROSU); // Banda 1
    tft.fillRect(100, 120, 8, 40, ROSU);  // Banda 2
    tft.fillRect(120, 120, 8, 40, ROSU); // Banda 3

  }
  else if (R2 >= 2350 && R2 < 2900)            // 2.7 kOhm
  { tft.fillRect(80, 120, 8, 40, ROSU); // Banda 1
    tft.fillRect(100, 120, 8, 40, VIOLET);  // Banda 2
    tft.fillRect(120, 120, 8, 40, ROSU); // Banda 3
  } 
  else if (R2 >= 3100 && R2 < 3500)            // 3.3 kOhm
  { tft.fillRect(80, 120, 8, 40, PORTOCALIU); // Banda 1
    tft.fillRect(100, 120, 8, 40, PORTOCALIU); // Banda 2
    tft.fillRect(120, 120, 8, 40, ROSU);   // Banda 3
  } 
  else if (R2 > 7000)                       // R inf
  {   tft.setTextColor(ALB);
      tft.setCursor(115, 130);                // centrat pe corp
      tft.print("?");
  }
 }
void deseneazaCondensator() {
  tft.fillRect(98, 130, 40, 8, GRI);     // fir stanga
  tft.fillRect(182, 130, 40, 8, GRI);    // fir dreapta
  tft.fillRect(138, 104, 12, 60, ALB);  // placa stanga
  tft.fillRect(170, 104, 12, 60, ALB);  // placa dreapta
 }

void deseneazaSoare(float lux) {
  tft.fillRect(90, 80, 140, 150, NEGRU);
  if (lux > 0 && lux < 999) {
  tft.drawCircle(160, 160, 15, GALBEN);
  tft.drawLine(160, 135, 160, 130, GALBEN);  // N
  tft.drawLine(160, 185, 160, 190, GALBEN);  // S
  tft.drawLine(135, 160, 130, 160, GALBEN);  // V
  tft.drawLine(185, 160, 190, 160, GALBEN);  // E
  tft.drawLine(142, 142, 139, 139, GALBEN);  // NW
  tft.drawLine(178, 142, 181, 139, GALBEN);  // NE
  tft.drawLine(142, 178, 139, 181, GALBEN);  // SW
  tft.drawLine(178, 178, 181, 181, GALBEN);  // SE
  } else if (lux >= 1000 && lux < 9999) {
  tft.drawCircle(160, 160, 15, GALBEN);
  tft.fillCircle(160, 160, 5, GALBEN);
  tft.drawLine(160, 135, 160, 125, GALBEN);  // N
  tft.drawLine(160, 185, 160, 195, GALBEN);  // S
  tft.drawLine(135, 160, 125, 160, GALBEN);  // V
  tft.drawLine(185, 160, 195, 160, GALBEN);  // E
  tft.drawLine(142, 142, 135, 135, GALBEN);  // NW
  tft.drawLine(178, 142, 185, 135, GALBEN);  // NE
  tft.drawLine(142, 178, 135, 185, GALBEN);  // SW
  tft.drawLine(178, 178, 185, 185, GALBEN);  // SE
  } else if (lux >= 10000 && lux < 14999) {
  tft.drawCircle(160, 160, 15, GALBEN);
  tft.fillCircle(160, 160, 10, GALBEN);
  tft.drawLine(160, 135, 160, 120, GALBEN);  // N
  tft.drawLine(160, 185, 160, 200, GALBEN);  // S
  tft.drawLine(135, 160, 120, 160, GALBEN);  // V
  tft.drawLine(185, 160, 200, 160, GALBEN);  // E
  tft.drawLine(142, 142, 125, 130, GALBEN);  // NW
  tft.drawLine(178, 142, 195, 130, GALBEN);  // NE
  tft.drawLine(142, 178, 125, 195, GALBEN);  // SW
  tft.drawLine(178, 178, 195, 195, GALBEN);  // SE
  } else if (lux >= 15000 && lux < 19999) {
  tft.drawCircle(160, 160, 15, GALBEN);
  tft.fillCircle(160, 160, 12, GALBEN);
  tft.drawLine(160, 135, 160, 115, GALBEN);  // N
  tft.drawLine(160, 185, 160, 205, GALBEN);  // S
  tft.drawLine(135, 160, 115, 160, GALBEN);  // V
  tft.drawLine(185, 160, 205, 160, GALBEN);  // E
  tft.drawLine(142, 142, 120, 130, GALBEN);  // NW
  tft.drawLine(178, 142, 200, 130, GALBEN);  // NE
  tft.drawLine(142, 178, 120, 200, GALBEN);  // SW
  tft.drawLine(178, 178, 200, 200, GALBEN);  // SE
  } else if (lux >= 20000 && lux < 24999) {
  tft.drawCircle(160, 160, 15, GALBEN);
  tft.fillCircle(160, 160, 14, GALBEN);
  tft.drawLine(160, 135, 160, 110, GALBEN);  // N
  tft.drawLine(160, 185, 160, 210, GALBEN);  // S
  tft.drawLine(135, 160, 110, 160, GALBEN);  // V
  tft.drawLine(185, 160, 210, 160, GALBEN);  // E
  tft.drawLine(142, 142, 115, 125, GALBEN);  // NW
  tft.drawLine(178, 142, 205, 125, GALBEN);  // NE
  tft.drawLine(142, 178, 115, 205, GALBEN);  // SW
  tft.drawLine(178, 178, 205, 205, GALBEN);  // SE
  } else if (lux >= 25000 && lux < 39000) {
  tft.drawCircle(160, 160, 15, GALBEN);
  tft.fillCircle(160, 160, 15, GALBEN);
  tft.drawLine(160, 135, 160, 105, GALBEN);  // N
  tft.drawLine(160, 185, 160, 215, GALBEN);  // S
  tft.drawLine(135, 160, 105, 160, GALBEN);  // V
  tft.drawLine(185, 160, 215, 160, GALBEN);  // E
  tft.drawLine(142, 142, 110, 125, GALBEN);  // NW
  tft.drawLine(178, 142, 210, 125, GALBEN);  // NE
  tft.drawLine(142, 178, 110, 205, GALBEN);  // SW
  tft.drawLine(178, 178, 210, 205, GALBEN);  // SE
  } else if (lux >= 39000 && lux <= 66000) {
  tft.drawCircle(160, 160, 15, GALBEN);
  tft.fillCircle(160, 160, 15, GALBEN);
  tft.drawLine(160, 135, 160, 100, GALBEN);  // N
  tft.drawLine(160, 185, 160, 220, GALBEN);  // S
  tft.drawLine(135, 160, 100, 160, GALBEN);  // V
  tft.drawLine(185, 160, 220, 160, GALBEN);  // E
  tft.drawLine(142, 142, 105, 120, GALBEN);  // NW
  tft.drawLine(178, 142, 215, 120, GALBEN);  // NE
  tft.drawLine(142, 178, 105, 205, GALBEN);  // SW
  tft.drawLine(178, 178, 215, 205, GALBEN);  // SE
  }
 }

void Informatii(){
   int citire_Pin = analogRead(batteryPin);
   long InternalVref_ADC_Value = readVcc(); 
   float Vcc_Real = (1023.0 * 1.1) / InternalVref_ADC_Value; 
   float tensiune_pe_pin = citire_Pin * (Vcc_Real / 1023.0);
   float tensiune_baterie = tensiune_pe_pin * 2.0;
   tft.fillRect(220, 30, 60, 30, NEGRU);
   tft.setCursor(10, 30);
   tft.print("Tensiune baterie: ");
   tft.print(tensiune_baterie, 2);
   tft.print(" V");
   tft.setCursor(10, 60);
   tft.print("ATmega328P 16 MHz");
   tft.setCursor(10, 90);
   tft.print("Memorie flash: 32 KB");

   if (tensiune_baterie < 3.1 ) {
    tft.setTextColor(ROSU); // Setează culoarea textului la roșu
    tft.setCursor(10, 120);
    tft.print("BATERIE DESCARCATA!");
    tft.setTextColor(ALB);   }    // La iesirea din functie textul revine la culoarea alba
   delay(300);}

long readVcc() {
  // Setează referința la Vcc și intrarea la referința internă de 1.1V
  // Aceasta este configurația specifică pentru ATmega328P (folosit pe Arduino Nano)
  ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  // Așteaptă puțin ca referința să se stabilizeze
  delay(2); 
  // Pornește conversia analog-digitală
  ADCSRA |= _BV(ADSC); 
  // Așteaptă până când conversia este completă
  while (bit_is_set(ADCSRA,ADSC)); 
  // Returnează rezultatul pe 10 biți
  return ADCW; 
}
