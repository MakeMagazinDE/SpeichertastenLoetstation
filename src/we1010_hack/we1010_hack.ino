/* Weller WE 1010 - Hack
 * 
 * Erweiterung einer Weller Loetstation WE 1010
 * Zwei Taster fuer voreingestellte Temperaturen
 * Langes Druecken auf die Tasten ermoeglicht das Aendern der im EEPROM gespeicherten Temperaturen
 * 
 * Make 6/2018, https://www.heise.de/make/
 * Florian Schäffer
 *  
*/

#include <EEPROM.h>

uint16_t Temp1 = 350;     // Wunsch-Temperatur Taster 1
uint16_t Temp2 = 400;     // Wunsch-Temperatur Taster 1

const uint8_t KeyUp   = 2;      // Pin Transistor aufwaerts
const uint8_t KeyDown = 3;      // Pin Transistor abwaerts
const uint8_t KeyTemp1 = A0;    // Pin Taster Temp1
const uint8_t KeyTemp2 = A1;    // Pin Taster Temp2
const uint16_t TempMin = 100;   // Minimal moegliche Temperatur
const uint16_t TempMax = 450;   // Maximal moegliche Temperatur
const uint8_t TLength = 70;     // ms Tastendruck (30/70)
const uint8_t TPause = 150;      // ms Pause zw. Tastendruck (60/150)
const uint16_t TMaxDiff = 4500; // ms von max Temp bis "Anschlag" min Temp
const uint8_t ledPin = 7;
volatile uint16_t TimerValue;

// Konstanten fuer blinkende LED
// Timer mit Teiler 1024
// ATmega mit 16.000.000 Hz Quarz
// Timer-Wert = 65535 - [Zeit in Sekunden] * CPU-Freq / Prescaler
const uint16_t LedNormal = 65535 - 2 * 16000000 / 1024.0;
const uint16_t LedFast = 65535 - 0.05 * 16000000 / 1024.0;
const uint16_t LedOn = 65530;   // blinkt irre schnell = Dauer An

void SetLedSpeed (uint16_t);
void KeyPressed (uint8_t);
void SetTemp (uint8_t);
void SetNewTemp (uint8_t);

void setup() {}   // unnuetz

/**
  @brief  Aendert die Blinkgeschwindigkeit der LED
  @param  Wert fuer Counter-Register oder 0=Aus
          Timer-Wert = 65535 - [Zeit in Sekunden] * CPU-Freq / Prescaler
  @return none
*/
void SetLedSpeed (uint16_t LedSpeed)
{
  digitalWrite(ledPin, LOW);
  
  // 16-Bit Timer 1 einrichten
  cli();                                  // Alle Interrupts temporär abschalten
  TCCR1A = 0;                             // Normal mode
  TCCR1B = 0;
  
  TimerValue = LedSpeed;
  TCNT1 = TimerValue;                     // Timer vorbelegen
  TIMSK1 |= (1 << TOIE1);                 // Timer Overflow Interrupt aktivieren

  if (LedSpeed)                             // bei 0 keinen Timer starten
    TCCR1B |= (1 << CS10) | (1 << CS12);    // 1024 als Prescale-Wert spezifizieren => Start
  sei();
}

/**
  @brief  Wenn eine Taste gedrueckt wurde: was soll jetzt passieren?
  @param  welche Taste wurde gedrueckt
  @return none
*/
void KeyPressed (uint8_t taste)
{
  uint32_t i;

  SetLedSpeed (LedOn);

  delay (10);   // entprellen

  i = millis();   // aktuelle Laufzeit
  while (!digitalRead(taste))      // solange gedrueckt
  {
    if (millis() >= (i + 4000))
        SetLedSpeed(0);
  }

  if (millis() >= (i + 4000))
    SetNewTemp (taste);
  else
    SetTemp (taste);

  SetLedSpeed(LedNormal);
}

/**
  @brief  Stellt die Temperatur am Geraet ein
  @param  welche Taste wurde gedrueckt
  @return none
*/
void SetTemp (uint8_t KeyTemp)
{
  uint16_t i;
  uint16_t TargetTemp;

  SetLedSpeed(LedFast);

  if (KeyTemp == KeyTemp1)
    TargetTemp = Temp1;
  else 
    TargetTemp = Temp2;

  if ((TempMax - TargetTemp) > (TargetTemp - TempMin))        // Zieltemp ist naeher an Minimum Temp => auf Minimum und dann hochzaehlen
  {
    digitalWrite(KeyDown, HIGH);                // An unteren Anschlag fahren
    delay(TMaxDiff);                     
    digitalWrite(KeyDown, LOW);  
    delay (100);
    
    for (i = TempMin; i < TargetTemp; i++)      // einzelne Gradschritte nach oben
    {
      digitalWrite(KeyUp, HIGH);  
      delay(TLength);                     
      digitalWrite(KeyUp, LOW);   
      delay(TPause);                  
    }
  }
  else
  {
    digitalWrite(KeyUp, HIGH);                // An oberen Anschlag fahren
    delay(TMaxDiff);                     
    digitalWrite(KeyUp, LOW);  
    delay (100);
    
    for (i = TempMax; i > TargetTemp; i--)      // einzelne Gradschritte nach unten
    {
      digitalWrite(KeyDown, HIGH);  
      delay(TLength);                     
      digitalWrite(KeyDown, LOW);   
      delay(TPause);                  
    }
  }
}

/**
  @brief  Ermoeglicht die Umprogrammierung der Tasten
  @param  welche Taste wurde gedrueckt
  @return none
*/
void SetNewTemp (uint8_t KeyTemp)
{
  uint32_t zeit;
  uint16_t NeueTemp = TempMin;
  
  SetLedSpeed(LedFast);
  
  digitalWrite(KeyDown, HIGH);                // An unteren Anschlag fahren
  delay(TMaxDiff);                     
  digitalWrite(KeyDown, LOW);  
  delay (100);

  SetLedSpeed(0);
  zeit = millis();
  while (millis() <= (zeit + 4000))     // solange zwischen letztem Tastendruck und Jetzt weniger als 4000 ms vergangen sind: warte
  {
    if (!digitalRead(KeyTemp))          // solange Taste gedrueckt => festhalten = Auto-Inkrement
    {
      digitalWrite(ledPin, HIGH);
      zeit = millis();                  // neuer Tastendruck => Countdown neu starten
      digitalWrite(KeyUp, HIGH);  
      delay(TLength);                     
      digitalWrite(KeyUp, LOW);   
      delay(TPause);                  
      NeueTemp++;
      digitalWrite(ledPin, LOW);
    }
  }

  if (KeyTemp == KeyTemp1)
  {
    EEPROM.write(0, (NeueTemp >> 8));   // Word im EEPROM speichern
    EEPROM.write(1, NeueTemp);
    Temp1 = NeueTemp;
  }
  else 
  {
    EEPROM.write(2, (NeueTemp >> 8));
    EEPROM.write(3, NeueTemp);
    Temp2 = NeueTemp;
  }
}

ISR(TIMER1_OVF_vect)        
{
  TCNT1 = TimerValue;             // Zähler erneut vorbelegen
  digitalWrite(ledPin, digitalRead(ledPin) ^ 1);  // LED toggle
}

// main()
void loop() 
{
  uint16_t temp;

  pinMode(KeyTemp1, INPUT_PULLUP);
  pinMode(KeyTemp2, INPUT_PULLUP);
  pinMode(KeyUp, OUTPUT);
  pinMode(KeyDown, OUTPUT);
  pinMode(ledPin, OUTPUT);  
  digitalWrite (KeyUp, LOW);
  digitalWrite (KeyDown, LOW);

  SetLedSpeed(LedNormal);

  temp = (EEPROM.read(0) << 8) | EEPROM.read(1);      // lies zwei Bytes ein => Word
  if (temp == 0xFFFF)                     // Neuer Chip = leeres EEPROM = 0xFF in allen Zellen => Initialisiere mit oben eingestellten Werten
  {
    EEPROM.write(0, (Temp1 >> 8));
    EEPROM.write(1, Temp1);
  }
  else
    Temp1 = temp;

  temp = (EEPROM.read(2) << 8) | EEPROM.read(3);      // lies zwei Bytes ein => Word
  if (temp == 0xFFFF)                     // Neuer Chip = leeres EEPROM = 0xFF in allen Zellen => Initialisiere mit oben eingestellten Werten
  {
    EEPROM.write(2, (Temp2 >> 8));
    EEPROM.write(3, Temp2);
  }
  else
    Temp2 = temp;

  while (1)   // endlos
  {
    if (!digitalRead(KeyTemp1))     // welche taste? Low Aktiv
      KeyPressed (KeyTemp1); 
    if (!digitalRead(KeyTemp2))     // welche taste? Low Aktiv
      KeyPressed (KeyTemp2); 
  }
}
