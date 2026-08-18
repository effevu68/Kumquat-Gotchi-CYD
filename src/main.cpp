#include <Arduino.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <SPIFFS.h>
#include <NimBLEDevice.h>
#include <WiFi.h>
#include "time.h"
#include "secrets.h"

// --- CONFIGURAZIONE WIFI ---
const char* ssid     = SECRET_SSID;      
const char* password = SECRET_PASS;
// ---------------------------

TFT_eSPI tft = TFT_eSPI(); 

// Variabili per il sensore
float temp_pianta = 0.0;
uint8_t umidita_terreno = 0;
uint32_t luce_pianta = 0;       
uint16_t fertilita_terreno = 0; 
uint8_t batteria_sensore = 0;

// MAC Address Flower Care (Sostituisci con il tuo)
std::string miFloraMac = "xx:xx:xx:xx:xx:xx";

// Impostazioni luminosità schermo (Scala da 0 a 255)
const int PWM_MINIMO = 8;
const int PWM_MASSIMO = 200;

// Dichiarazione funzioni
void drawBmp(const char *filename, int16_t x, int16_t y);
bool leggiMiFlora();
void aggiornaSchermo();
void syncOrologio();
void gestisciLuminosita(struct tm &timeinfo);

void setup() {
  Serial.begin(115200);

  // Setup Display
  tft.init();
  tft.setRotation(0); 
  tft.invertDisplay(true); 
  
  // Setup PWM per la retroilluminazione (Nuovo standard ESP32 Core 3.x)
  ledcAttach(21, 5000, 8);  // Sostituisce ledcSetup e ledcAttachPin
  ledcWrite(21, PWM_MASSIMO); // Ora vuole il pin (21) e non più il canale

  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextDatum(MC_DATUM);

  // Inizializzazione Memoria Interna (SPIFFS)
  if (!SPIFFS.begin(true)) {
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.drawString("Errore Memoria Interna!", 120, 140, 2);
    Serial.println("Errore montaggio SPIFFS");
  } else {
    Serial.println("Memoria SPIFFS montata con successo");
  }

  // Sincronizzazione WiFi e Orologio
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("Connessione WiFi in corso...", 120, 160, 2);
  syncOrologio();

  // Inizializzazione Bluetooth a basso consumo
  NimBLEDevice::init("");
  
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("Cerco il Kumquat...", 120, 160, 2);
  
  leggiMiFlora();
  aggiornaSchermo();
}

void loop() {
  delay(300000); // Aggiorna ogni 5 minuti
  leggiMiFlora();
  aggiornaSchermo();
}

// --- FUNZIONE PER CONNETTERSI E PRENDERE L'ORA ESATTA ---
void syncOrologio() {
  WiFi.begin(ssid, password);
  int tentativi = 0;
  while (WiFi.status() != WL_CONNECTED && tentativi < 20) {
    delay(500);
    tentativi++;
  }
  
  if(WiFi.status() == WL_CONNECTED) {
    configTzTime("CET-1CEST,M3.5.0/2,M10.5.0/3", "pool.ntp.org", "time.nist.gov");
    Serial.println("WiFi Connesso e Orologio Sincronizzato!");
  } else {
    Serial.println("Errore WiFi, orologio non impostato.");
  }
}

// --- FUNZIONE PER LEGGERE IL SENSORE XIAOMI ---
bool leggiMiFlora() {
  NimBLEClient* pClient = NimBLEDevice::createClient();
  if(!pClient->connect(miFloraMac)) {
    NimBLEDevice::deleteClient(pClient);
    return false;
  }
  NimBLERemoteService* pService = pClient->getService("1204");
  if(pService) {
    NimBLERemoteCharacteristic* pMode = pService->getCharacteristic("1a00");
    NimBLERemoteCharacteristic* pData = pService->getCharacteristic("1a01");
    
    if(pMode && pData) {
      uint8_t magic[] = {0xA0, 0x1F}; 
      pMode->writeValue(magic, 2, true);
      delay(500); 
      
      // Lettura dei dati (Sensori)
      std::string value = pData->readValue();
      if(value.length() >= 16) {
        const uint8_t* p = (const uint8_t*)value.data();
        int16_t t = p[0] | (p[1] << 8);
        temp_pianta = t / 10.0f;
        luce_pianta = p[3] | (p[4] << 8) | (p[5] << 16) | (p[6] << 24);
        umidita_terreno = p[7];
        fertilita_terreno = p[8] | (p[9] << 8);
      }

      // Lettura dei dati (Batteria)
      NimBLERemoteCharacteristic* pBat = pService->getCharacteristic("1a02");
      if(pBat) {
        std::string batVal = pBat->readValue();
        if(batVal.length() >= 1) {
          batteria_sensore = batVal[0];
        }
      }

      pClient->disconnect();
      NimBLEDevice::deleteClient(pClient);
      return true;
    }
  }
  pClient->disconnect();
  NimBLEDevice::deleteClient(pClient);
  return false;
}

// --- FUNZIONE PER GESTIRE IL "DIMMER" DEL DISPLAY ---
void gestisciLuminosita(struct tm &timeinfo) {
  int ora = timeinfo.tm_hour;
  int minuti = timeinfo.tm_min;
  int minuti_odierni = (ora * 60) + minuti; 

  int inizio_alba = 6 * 60 + 30;    
  int fine_alba = 7 * 60;           
  int inizio_tramonto = 20 * 60 + 30; 
  int fine_tramonto = 21 * 60;        

  int luce_attuale = PWM_MASSIMO;

  if (minuti_odierni >= fine_tramonto || minuti_odierni < inizio_alba) {
    luce_attuale = PWM_MINIMO;
  }
  else if (minuti_odierni >= fine_alba && minuti_odierni < inizio_tramonto) {
    luce_attuale = PWM_MASSIMO;
  }
  else if (minuti_odierni >= inizio_alba && minuti_odierni < fine_alba) {
    int minuti_passati = minuti_odierni - inizio_alba;
    luce_attuale = map(minuti_passati, 0, 30, PWM_MINIMO, PWM_MASSIMO);
  }
  else if (minuti_odierni >= inizio_tramonto && minuti_odierni < fine_tramonto) {
    int minuti_passati = minuti_odierni - inizio_tramonto;
    luce_attuale = map(minuti_passati, 0, 30, PWM_MASSIMO, PWM_MINIMO);
  }

  ledcWrite(21, luce_attuale); // Aggiornato con il pin 21 invece del canale 0
}

// --- FUNZIONE PER AGGIORNARE LA GRAFICA ---
void aggiornaSchermo() {
  tft.fillScreen(TFT_BLACK);

  // Intestazione
  tft.fillRect(0, 0, 240, 30, TFT_DARKGREEN);
  tft.setTextColor(TFT_WHITE, TFT_DARKGREEN);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("KUMQUAT-GOTCHI", 120, 15, 4);

  // Prendiamo l'ora
  struct tm timeinfo;
  bool is_notte = false;
  if (getLocalTime(&timeinfo)) {
    gestisciLuminosita(timeinfo);

    int ora = timeinfo.tm_hour;
    int minuti = timeinfo.tm_min;
    
    if (ora >= 21 || ora < 6) is_notte = true;
    else if (ora == 20 && minuti >= 30) is_notte = true;
    else if (ora == 6) is_notte = true;
  }

  // Logica immagini (ora pesca direttamente dalla root della memoria interna)
  const char* percorso_img = "/felice.bmp";
  const char* stato_testo = "Sto benone! (^^)";
  uint16_t colore_testo = TFT_GREEN;

  if (umidita_terreno < 12) { 
    percorso_img = "/sete.bmp";
    stato_testo = "Ho sete! Annaffiami! (T_T)";
    colore_testo = TFT_RED;
  } else if (fertilita_terreno < 150) {
    percorso_img = "/fame.bmp";
    stato_testo = "Ho fame! Concime? (u_u)";
    colore_testo = TFT_ORANGE;
  } else if (is_notte) {
    percorso_img = "/felice.bmp"; 
    stato_testo = "Ronf... ronf... (Zzz)";
    colore_testo = TFT_SKYBLUE;
  } else if (luce_pianta < 300) {
    percorso_img = "/luce.bmp";
    stato_testo = "Voglio il sole! (*_*)";
    colore_testo = TFT_YELLOW;
  }

  // Carica l'immagine
  drawBmp(percorso_img, 40, 45);

  // Testo di stato
  tft.setTextColor(colore_testo, TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.drawString(stato_testo, 120, 220, 2);

  // Separatore
  tft.drawFastHLine(10, 235, 220, TFT_DARKGREY);

  // Testi Sensori
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextDatum(TL_DATUM); 
  
  char buffer[32];
  sprintf(buffer, "Umid: %u%%", umidita_terreno);
  tft.drawString(buffer, 15, 245, 2);
  
  sprintf(buffer, "Temp: %.1fC", temp_pianta);
  tft.drawString(buffer, 130, 245, 2);

  sprintf(buffer, "Fert: %u", fertilita_terreno);
  tft.drawString(buffer, 15, 275, 2);
  
  sprintf(buffer, "Luce: %lulx", luce_pianta); 
  tft.drawString(buffer, 130, 275, 2);

  // --- BATTERIA IN FONDO AL CENTRO ---
  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  char batStr[32];
  sprintf(batStr, "Batteria Sensore: %d%%", batteria_sensore);
  tft.drawString(batStr, 120, 305, 2);
}

// --- FUNZIONI DI SUPPORTO PER LETTURA BMP ---
uint16_t read16(File &f) {
  uint16_t result;
  ((uint8_t *)&result)[0] = f.read();
  ((uint8_t *)&result)[1] = f.read();
  return result;
}

uint32_t read32(File &f) {
  uint32_t result;
  ((uint8_t *)&result)[0] = f.read();
  ((uint8_t *)&result)[1] = f.read();
  ((uint8_t *)&result)[2] = f.read();
  ((uint8_t *)&result)[3] = f.read();
  return result;
}

// --- MOTORE BLINDATO PER LEGGERE I BMP DALLA MEMORIA INTERNA ---
void drawBmp(const char *filename, int16_t x, int16_t y) {
  File bmpFS = SPIFFS.open(filename, "r");
  if (!bmpFS) {
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("File non trovato!", 120, 120, 2);
    return;
  }
  
  if (read16(bmpFS) != 0x4D42) {
    bmpFS.close();
    return;
  }

  read32(bmpFS); 
  read32(bmpFS); 
  uint32_t imageOffset = read32(bmpFS); 
  read32(bmpFS); 
  int32_t w = read32(bmpFS);
  int32_t h = read32(bmpFS);
  int16_t planes = read16(bmpFS);
  int16_t depth = read16(bmpFS);

  if (planes == 1 && (depth == 24 || depth == 32)) {
    uint32_t rowSize = (w * depth / 8 + 3) & ~3;
    uint8_t lineBuffer[rowSize]; 
    boolean flip = true;

    if (h < 0) { h = -h; flip = false; }

    int w_render = w;
    int h_render = h;
    if (x + w > tft.width()) w_render = tft.width() - x;
    if (y + h > tft.height()) h_render = tft.height() - y;

    for (int row = 0; row < h_render; row++) {
      int pos = imageOffset + (flip ? (h - 1 - row) : row) * rowSize;
      bmpFS.seek(pos);
      bmpFS.read(lineBuffer, rowSize);
      
      int buffidx = 0;
      for (int col = 0; col < w_render; col++) {
        uint8_t b = lineBuffer[buffidx++];
        uint8_t g = lineBuffer[buffidx++];
        uint8_t r = lineBuffer[buffidx++];
        if (depth == 32) buffidx++; 
        
        tft.drawPixel(x + col, y + row, tft.color565(r, g, b));
      }
    }
  }
  bmpFS.close();
}
