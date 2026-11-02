#include <Arduino.h>
#include <math.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <Adafruit_NeoPixel.h>


// ==================================================
// OLED
// ==================================================

#define SDA_PIN 21
#define SCL_PIN 22

U8G2_SH1106_128X64_NONAME_F_HW_I2C display(
    U8G2_R0,
    U8X8_PIN_NONE
);


// ==================================================
// WS2812B RING
// ==================================================

#define RING_DATA 12
#define RING_LEDS 12

Adafruit_NeoPixel ring(
    RING_LEDS,
    RING_DATA,
    NEO_GRB + NEO_KHZ800
);

const int RING_HELLIGKEIT = 50;


// ==================================================
// AMPEL
// ==================================================

const int LED_ROT   = 13;
const int LED_GRUEN = 33;
const int LED_GELB  = 32;

unsigned long letzteAmpelZeit = 0;
const unsigned long AMPEL_ZEIT = 500;

int schritt = 0;


// ==================================================
// DURCHLAUF-ZÄHLER
// ==================================================

int zaehler = 0;

unsigned long letzterDurchlauf = 0;
const unsigned long DURCHLAUF_ZEIT = 1000;


// ==================================================
// TASTER
// ==================================================

const int TASTER_ROT   = 36;
const int TASTER_GRUEN = 34;

bool letzterRot   = HIGH;
bool letzterGruen = HIGH;

bool stabilRot   = HIGH;
bool stabilGruen = HIGH;

unsigned long letzteZeitRot   = 0;
unsigned long letzteZeitGruen = 0;

const unsigned long ENT_PRELLZEIT = 50;


// ==================================================
// RUNDEN
// ==================================================

int runden = 0;


// ==================================================
// POTI + LDR
// ==================================================

const int POTI = 35;
const int LDR  = 39;

unsigned long letzteSensorZeit = 0;
const unsigned long SENSOR_ZEIT = 200;


// ==================================================
// ULTRASCHALL
// ==================================================

const int TRIG = 14;
const int ECHO = 27;

unsigned long letzteUltraschallZeit = 0;
const unsigned long ULTRASCHALL_ZEIT = 100;


// ==================================================
// DURCHFAHRT / HYSTERESE
// ==================================================

const float SCHWELLWERT = 20.0;
const float HYSTERESE   = 25.0;

bool objektErkannt = false;


// ==================================================
// OLED
// ==================================================

float letzterDisplayAbstand = -1;


// ==================================================
// RING STATUS
// ==================================================

enum RingStatus
{
    STATUS_BEREIT,      // 🟢 System bereit
    STATUS_ERKANNT,     // 🟡 Durchfahrt erkannt
    STATUS_MESSUNG,     // 🔴 Messung läuft
    STATUS_ERGEBNIS     // 🔵 Ergebnis vorliegend
};

RingStatus aktuellerStatus = STATUS_BEREIT;


// ==================================================
// RING ANIMATION
// ==================================================

unsigned long letzterRingSchritt = 0;

const unsigned long RING_ANIMATION_ZEIT = 80;

int ringPosition = 0;

bool ringAnimationAktiv = false;


// ==================================================
// RING FARBEN
// ==================================================

uint32_t farbeGruen()
{
    return ring.Color(0, 255, 0);
}


uint32_t farbeGelb()
{
    return ring.Color(255, 150, 0);
}


uint32_t farbeRot()
{
    return ring.Color(255, 0, 0);
}


uint32_t farbeBlau()
{
    return ring.Color(0, 0, 255);
}


// ==================================================
// RING KOMPLETT FÄRBEN
// ==================================================

void ringFarbe(uint32_t farbe)
{
    ring.fill(farbe);
    ring.show();
}


// ==================================================
// RING STATUS SETZEN
// ==================================================

void setzeRingStatus(RingStatus status)
{
    aktuellerStatus = status;

    // Lauflicht stoppen.
    // Der Status soll dauerhaft sichtbar bleiben.
    ringAnimationAktiv = false;

    if (status == STATUS_BEREIT)
    {
        // 🟢 Grün = bereit
        ringFarbe(farbeGruen());
    }

    else if (status == STATUS_ERKANNT)
    {
        // 🟡 Gelb = Durchfahrt erkannt
        ringFarbe(farbeGelb());
    }

    else if (status == STATUS_MESSUNG)
    {
        // 🔴 Rot = Messung läuft
        ringFarbe(farbeRot());
    }

    else if (status == STATUS_ERGEBNIS)
    {
        // 🔵 Blau = Ergebnis vorliegend
        ringFarbe(farbeBlau());
    }
}


// ==================================================
// RING-LAUFLICHT STARTEN
// ==================================================

void starteRingAnimation()
{
    ringAnimationAktiv = true;

    ringPosition = 0;

    letzterRingSchritt = millis();

    ring.clear();
    ring.show();
}


// ==================================================
// RING-LAUFLICHT
// NICHT BLOCKIEREND
// ==================================================

void aktualisiereRingAnimation()
{
    if (!ringAnimationAktiv)
    {
        return;
    }

    unsigned long jetzt = millis();

    if (jetzt - letzterRingSchritt >= RING_ANIMATION_ZEIT)
    {
        letzterRingSchritt = jetzt;

        ring.clear();

        // Eine blaue LED wandert um den Ring.
        ring.setPixelColor(
            ringPosition,
            farbeBlau()
        );

        ring.show();

        ringPosition++;

        if (ringPosition >= RING_LEDS)
        {
            ringPosition = 0;
        }
    }
}


// ==================================================
// ULTRASCHALL EINZELMESSUNG
// ==================================================

float messeDistanz()
{
    digitalWrite(TRIG, LOW);

    delayMicroseconds(2);

    digitalWrite(TRIG, HIGH);

    delayMicroseconds(10);

    digitalWrite(TRIG, LOW);


    long echoZeit = pulseIn(
        ECHO,
        HIGH,
        30000
    );


    // Kein Echo
    if (echoZeit == 0)
    {
        return 400.0;
    }


    // Schallgeschwindigkeit:
    // 0.0343 cm/us
    //
    // Der Schall läuft hin und zurück,
    // deshalb /2.

    float abstand =
        echoZeit * 0.0343 / 2.0;


    return abstand;
}


// ==================================================
// 3 MESSUNGEN + MITTELWERT
// ==================================================

float messeDistanzMittelwert()
{
    float messung1 = messeDistanz();

    delay(5);

    float messung2 = messeDistanz();

    delay(5);

    float messung3 = messeDistanz();


    float mittelwert =
        (
            messung1 +
            messung2 +
            messung3
        ) / 3.0;


    return mittelwert;
}


// ==================================================
// STRECKE, RUNDENMESSUNG UND OLED
// ==================================================

float aktuellerAbstand = 0.0;
bool abstandVorhanden = false;

// Poti an GPIO 35: Strecke von 10 bis 500 cm
int streckeCm = 100;
int letzterPotiWert = -1;

float aktuelleGeschwindigkeit = 0.0;
bool geschwindigkeitVorhanden = false;

unsigned long rundenStartZeit = 0;
unsigned long rundenZeitMs = 0;

bool rundeLaeuft = false;
bool ergebnisFesthalten = false;

unsigned long letzteOLEDZeit = 0;
const unsigned long OLED_ZEIT = 100;


// ==================================================
// NEUE RUNDE ZURÜCKSETZEN
// ==================================================

void setzeNeueMessung()
{
    objektErkannt = false;
    abstandVorhanden = false;
    geschwindigkeitVorhanden = false;

    aktuelleGeschwindigkeit = 0.0;
    rundenStartZeit = 0;
    rundenZeitMs = 0;

    rundeLaeuft = false;
    ergebnisFesthalten = false;
}


// ==================================================
// STATUS-TEXT
// ==================================================

const char* statusText()
{
    if (ergebnisFesthalten)
        return "RUNDE FERTIG - GRUEN NEU";

    if (rundeLaeuft)
        return "RUNDE LAEUFT - LIVE";

    return "BEREIT - FAHRZEUG ERWARTET";
}


// ==================================================
// OLED AKTUALISIEREN
// ==================================================

void aktualisiereOLED()
{
    unsigned long jetzt = millis();

    if (jetzt - letzteOLEDZeit < OLED_ZEIT)
        return;

    letzteOLEDZeit = jetzt;

    display.clearBuffer();

    display.setFont(u8g2_font_6x10_tf);
    display.drawStr(0, 9, "MESSSTATION");
    display.drawHLine(0, 12, 128);

    // Abstand
    display.setFont(u8g2_font_5x8_tf);
    display.drawStr(0, 22, "ABSTAND");

    char abstandText[20];

    if (abstandVorhanden)
        snprintf(abstandText, sizeof(abstandText),
                 "%.1f cm", aktuellerAbstand);
    else
        snprintf(abstandText, sizeof(abstandText),
                 "---.- cm");

    display.setFont(u8g2_font_ncenB12_tr);
    display.drawStr(0, 39, abstandText);

    // Geschwindigkeit
    char speedText[24];

    if (geschwindigkeitVorhanden)
    {
        snprintf(speedText, sizeof(speedText),
                 "v %.2f m/s", aktuelleGeschwindigkeit);
    }
    else if (rundeLaeuft)
    {
        float sekunden =
            (jetzt - rundenStartZeit) / 1000.0f;

        float liveSpeed = 0.0f;

        if (sekunden > 0.05f)
        {
            liveSpeed = (streckeCm / 100.0f) / sekunden;
        }

        snprintf(speedText, sizeof(speedText),
                 "v %.2f m/s", liveSpeed);
    }
    else
    {
        snprintf(speedText, sizeof(speedText),
                 "v --.-- m/s");
    }

    display.setFont(u8g2_font_6x10_tf);
    display.drawStr(65, 38, speedText);

    // Eingestellte Strecke und Rundenzähler
    char streckeText[24];

    snprintf(streckeText, sizeof(streckeText),
             "Strecke:%dcm R:%d", streckeCm, runden);

    display.setFont(u8g2_font_5x8_tf);
    display.drawStr(0, 49, streckeText);

    display.drawHLine(0, 53, 128);

    display.setFont(u8g2_font_4x6_tf);
    display.drawStr(0, 62, statusText());

    display.sendBuffer();
}


// ==================================================
// POTI AUSLESEN
// ==================================================

void aktualisierePoti()
{
    int potiWert = analogRead(POTI);

    // Strecke nur zwischen den Runden verändern.
    if (!rundeLaeuft && !ergebnisFesthalten)
    {
        streckeCm = map(potiWert, 0, 4095, 10, 500);
        streckeCm = constrain(streckeCm, 10, 500);
    }

    if (potiWert != letzterPotiWert)
    {
        letzterPotiWert = potiWert;

        Serial.print("Poti: ");
        Serial.print(potiWert);

        Serial.print(" | Strecke: ");
        Serial.print(streckeCm);

        Serial.println(" cm");
    }
}
// ==================================================
// SETUP
// ==================================================

void setup()
{
    // ==================================================
    // SERIAL
    // ==================================================

    Serial.begin(9600);


    // ==================================================
    // AMPEL-LEDS
    // ==================================================

    pinMode(
        LED_ROT,
        OUTPUT
    );

    pinMode(
        LED_GRUEN,
        OUTPUT
    );

    pinMode(
        LED_GELB,
        OUTPUT
    );


    // ==================================================
    // TASTER
    // ==================================================

    pinMode(
        TASTER_ROT,
        INPUT
    );

    pinMode(
        TASTER_GRUEN,
        INPUT
    );


    // ==================================================
    // ANALOG
    // ==================================================

    analogReadResolution(12);


    // ==================================================
    // ULTRASCHALL
    // ==================================================

    pinMode(
        TRIG,
        OUTPUT
    );

    pinMode(
        ECHO,
        INPUT
    );

    digitalWrite(
        TRIG,
        LOW
    );


    // ==================================================
    // I2C
    // ==================================================

    Wire.begin(
        SDA_PIN,
        SCL_PIN
    );


    // ==================================================
    // OLED
    // ==================================================

    display.begin();


    // ==================================================
    // OLED STARTANZEIGE
    // ==================================================

    display.clearBuffer();

    display.setFont(u8g2_font_ncenB08_tr);
    display.drawStr(0, 15, "Planlos aber");
    display.drawStr(0, 30, "Normgerecht");

    display.drawHLine(0, 36, 128);

    display.setFont(u8g2_font_6x10_tf);
    display.drawStr(0, 51, "MESSSTATION");
    display.drawStr(0, 63, "System startet...");

    display.sendBuffer();


    // ==================================================
    // WS2812B RING
    // ==================================================

    ring.begin();

    ring.setBrightness(
        RING_HELLIGKEIT
    );

    ring.clear();

    ring.show();


    // ==================================================
    // SYSTEM BEREIT
    // ==================================================

    setzeRingStatus(
        STATUS_BEREIT
    );


    delay(1800);

    // Danach wird nur noch die strukturierte Messanzeige gezeigt.
    letzteOLEDZeit = millis() - OLED_ZEIT;
}


// ==================================================
// LOOP
// ==================================================

void loop()
{
    unsigned long jetzt = millis();
    


    // ==================================================
    // 1. RING-LAUFLICHT
    // ==================================================

    aktualisiereRingAnimation();


    // ==================================================
    // 2. AMPEL
    // ==================================================

    if (
        jetzt - letzteAmpelZeit >=
        AMPEL_ZEIT
    )
    {
        letzteAmpelZeit = jetzt;

        schritt++;


        if (schritt > 3)
        {
            schritt = 0;
        }


        // -------------------------
        // SCHRITT 0
        // -------------------------

        if (schritt == 0)
        {
            digitalWrite(
                LED_ROT,
                LOW
            );

            digitalWrite(
                LED_GRUEN,
                HIGH
            );

            digitalWrite(
                LED_GELB,
                HIGH
            );
        }


        // -------------------------
        // SCHRITT 1
        // -------------------------

        else if (schritt == 1)
        {
            digitalWrite(
                LED_ROT,
                LOW
            );

            digitalWrite(
                LED_GRUEN,
                HIGH
            );

            digitalWrite(
                LED_GELB,
                LOW
            );
        }


        // -------------------------
        // SCHRITT 2
        // -------------------------

        else if (schritt == 2)
        {
            digitalWrite(
                LED_ROT,
                HIGH
            );

            digitalWrite(
                LED_GRUEN,
                LOW
            );

            digitalWrite(
                LED_GELB,
                HIGH
            );
        }


        // -------------------------
        // SCHRITT 3
        // -------------------------

        else if (schritt == 3)
        {
            digitalWrite(
                LED_ROT,
                HIGH
            );

            digitalWrite(
                LED_GRUEN,
                HIGH
            );

            digitalWrite(
                LED_GELB,
                LOW
            );
        }
    }


    // ==================================================
    // 3. DURCHLAUF NR.
    // JEDE SEKUNDE
    // ==================================================

    if (
        jetzt - letzterDurchlauf >=
        DURCHLAUF_ZEIT
    )
    {
        letzterDurchlauf = jetzt;

        zaehler++;

        Serial.print(
            "Durchlauf Nr. "
        );

        Serial.println(
            zaehler
        );
    }


    // ==================================================
    // 4. ROTER TASTER
    // ==================================================

    bool jetztRot =
        digitalRead(
            TASTER_ROT
        );


    if (
        jetztRot != letzterRot
    )
    {
        letzteZeitRot = jetzt;

        letzterRot = jetztRot;
    }


    if (
        jetzt - letzteZeitRot >=
        ENT_PRELLZEIT
    )
    {
        if (
            jetztRot != stabilRot
        )
        {
            stabilRot = jetztRot;


            if (
                stabilRot == LOW
            )
            {
                Serial.println(
                    "Taster ROT gedrueckt"
                );


// Roter Taster: Rundenzähler zurücksetzen
runden = 0;
setzeNeueMessung();

Serial.println("Runden: 0");
Serial.println("Bereit fuer neue Runde");

setzeRingStatus(STATUS_BEREIT);
            }
        }
    }


    // ==================================================
    // 5. GRÜNER TASTER
    // ==================================================

    bool jetztGruen =
        digitalRead(
            TASTER_GRUEN
        );


    if (
        jetztGruen != letzterGruen
    )
    {
        letzteZeitGruen = jetzt;

        letzterGruen = jetztGruen;
    }


    if (
        jetzt - letzteZeitGruen >=
        ENT_PRELLZEIT
    )
    {
        if (
            jetztGruen != stabilGruen
        )
        {
            stabilGruen = jetztGruen;


            if (
                stabilGruen == LOW
            )
            {
                Serial.println(
                    "Taster GRUEN gedrueckt"
                );


                // Neue Runde: Zähler erhöhen und Live-Messung zurücksetzen
                runden++;
                setzeNeueMessung();

                Serial.print("Runden: ");
                Serial.println(runden);
                Serial.println("Neue Runde: Live-Messmodus aktiv");

                setzeRingStatus(STATUS_MESSUNG);
            }
        }
    }


// ==================================================
// 6. POTI + LDR
// ==================================================

if (jetzt - letzteSensorZeit >= SENSOR_ZEIT)
{
    letzteSensorZeit = jetzt;

    aktualisierePoti();

    int ldrWert = analogRead(LDR);

    Serial.print("LDR:");
    Serial.println(ldrWert);
}
    // ==================================================
    // 7. ULTRASCHALL
    // ALLE 100 ms
    // ==================================================

    if (
        jetzt - letzteUltraschallZeit >=
        ULTRASCHALL_ZEIT
    )
    {
        letzteUltraschallZeit = jetzt;


        // ------------------------------------------
        // 3 Messungen + Mittelwert
        // ------------------------------------------

        float abstand =
            messeDistanzMittelwert();


        // ------------------------------------------
        // Serial
        // ------------------------------------------

        Serial.print(
            "Abstand:"
        );

        Serial.println(
            abstand
        );


       // ==================================================
// ABSTAND LIVE ANZEIGEN
// ==================================================

if (!ergebnisFesthalten &&
    abstand > 0.0 &&
    abstand < 400.0)
{
    aktuellerAbstand = abstand;
    abstandVorhanden = true;
}


// ==================================================
// ERSTE ODER ZWEITE PASSAGE ERKENNEN
// ==================================================

if (!ergebnisFesthalten &&
    !objektErkannt &&
    abstand < SCHWELLWERT)
{
    objektErkannt = true;

    // ----------------------------------------------
    // ERSTE PASSAGE: TIMER STARTEN
    // ----------------------------------------------

    if (!rundeLaeuft)
    {
        rundeLaeuft = true;
        rundenStartZeit = jetzt;
        rundenZeitMs = 0;

        geschwindigkeitVorhanden = false;
        aktuelleGeschwindigkeit = 0.0f;

        Serial.println(
            "Erste Passage - Zeitmessung gestartet"
        );

        setzeRingStatus(STATUS_MESSUNG);
    }

    // ----------------------------------------------
    // ZWEITE PASSAGE: RUNDE BEENDET
    // ----------------------------------------------

    else
    {
        rundenZeitMs = jetzt - rundenStartZeit;

        rundeLaeuft = false;
        ergebnisFesthalten = true;

        if (rundenZeitMs > 0)
        {
            float zeitSekunden =
                rundenZeitMs / 1000.0f;

            float streckeMeter =
                streckeCm / 100.0f;

            aktuelleGeschwindigkeit =
                streckeMeter / zeitSekunden;

            geschwindigkeitVorhanden = true;
        }

        runden++;

        Serial.println("Runde beendet!");
        

        Serial.print("Strecke: ");
        Serial.print(streckeCm);
        Serial.println(" cm");

        Serial.print("Rundenzeit: ");
        Serial.print(rundenZeitMs / 1000.0f, 2);
        Serial.println(" Sekunden");

        Serial.print("Durchschnittsgeschwindigkeit: ");

        if (geschwindigkeitVorhanden)
        {
            Serial.print(aktuelleGeschwindigkeit, 2);
            Serial.println(" m/s");
        }
        else
        {
            Serial.println("nicht berechenbar");
        }

        setzeRingStatus(STATUS_ERGEBNIS);
    }
}


// ==================================================
// SENSORBEREICH WIEDER FREI
// ==================================================

if (objektErkannt && abstand > HYSTERESE)
{
    objektErkannt = false;
    Serial.println("Sensorbereich wieder frei");
}

        aktualisiereOLED();
    }
}