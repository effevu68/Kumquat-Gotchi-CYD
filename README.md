# 🍊 Kumquat-Gotchi: Il Tamagotchi per Piante su CYD 2.8"

Trasforma il monitoraggio della tua pianta in un simpatico Tamagotchi interattivo! 
Questo progetto usa un **Cheap Yellow Display (CYD) da 2.8"** (ESP32-2432S028R) e un sensore Bluetooth **Xiaomi Mi Flora / Flower Care** per leggere i parametri vitali di un mandarino cinese (o qualsiasi altra pianta) e mostrare faccine diverse a seconda delle sue necessità.

---

## 🌟 Funzionalità

* **Lettura BLE:** Si collega in automatico al sensore Xiaomi Mi Flora per leggere umidità del terreno, temperatura, fertilità, luce e livello della batteria del sensore.
* **Smart Dimmer (Ciclo Giorno/Notte):** Il display gestisce la retroilluminazione (PWM) in base all'orologio interno (sincronizzato via NTP). Dalle 6:30 fa "l'alba" (alzando la luce gradualmente), e dalle 20:30 fa il "tramonto", abbassando la luminosità per la notte per non disturbare e non usurare lo schermo.
* **Faccine Dinamiche (File BMP):** Il sistema legge delle piccole immagini `.bmp` direttamente dalla memoria interna (SPIFFS) dell'ESP32 per mostrare l'umore della pianta:
  * 💧 **Sete:** Se l'umidità scende sotto il 12%.
  * 🍔 **Fame:** Se il concime scende sotto i 150.
  * ☀️ **Luce:** Se la luminosità ambientale è sotto i 300 lux.
  * 💤 **Nanna:** Durante l'orario notturno.
  * 😃 **Felice:** Se tutti i parametri sono ottimali!

---

## 🛠️ Requisiti Software e Installazione

Il progetto è sviluppato su **PlatformIO** (VS Code) sfruttando il core Arduino ESP32 aggiornato (versione 3.x), che utilizza i nuovi comandi `ledcAttach` e `ledcWrite` per la retroilluminazione.

1. Clonate questa repository.
2. Create un file chiamato `secrets.h` nella stessa cartella del `main.cpp` (questo file NON deve essere caricato online per la vostra sicurezza). Il file deve contenere queste due righe:
   ```cpp
   const char* SECRET_SSID = "IL_TUO_WIFI";      
   const char* SECRET_PASS = "LA_TUA_PASSWORD";

Nel file main.cpp, modificate la variabile miFloraMac inserendo l'indirizzo MAC del vostro sensore Xiaomi.

Caricate le vostre immagini (felice.bmp, sete.bmp, ecc.) nella memoria SPIFFS del CYD tramite l'apposito tool di PlatformIO.

Compilate e caricate il codice!

📌 Note Tecniche
Il touchscreen XPT2046 in questo progetto è stato disabilitato via software (-D TOUCH_CS=-1 nel platformio.ini) in quanto non necessario, risparmiando risorse.

Assicuratevi di inserire nel CYD una scheda MicroSD funzionante se decidete di spostare la lettura dei file BMP dallo SPIFFS alla SD (il codice attuale è ottimizzato per la memoria interna SPIFFS).
