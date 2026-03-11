//
// updated by ...: Loreto Notarantonio
// Date .........: 20-02-2026 17.54.06
//

#include <Arduino.h>
#include "lnLogger_Class.h"
#include "lnWiFiManager.h"

#define  __I_AM_MAIN_CPP__
#include <ssid_credentials_esp32.h>

lnWiFiManagerNB wifiManager;

// Variabili di stato
#define BUTTON_PIN 19
bool canUseNetwork = false;
uint32_t lastRetryTime = 0;
const uint32_t retryInterval = 30000; // 30 secondi tra i tentativi di scansione se disconnesso

// --- CALLBACK: Qui gestiamo gli eventi di rete
void onConnectionChanged(bool connected) {
    canUseNetwork = connected;

    if (connected) {
        lnLOG_NOTIFY("SISTEMA: Rete ripristinata. Avvio servizi...");
        // Qui puoi chiamare funzioni "una tantum" al momento della connessione:
        // configTime(gmtOffset_sec, daylightOffset_sec, "pool.ntp.org");
        // myTelegramBot.sendMessage(CHAT_ID, "Sistema Online!", "");
    } else {
        lnLOG_ERROR("SISTEMA: Connessione persa. Servizi in pausa.");
    }
}



// #########################################################
// #
// #########################################################
void setup() {
    Serial.begin(115200);
    lnLog.init(128, 25);

    // Configura il PIN di test
    pinMode(BUTTON_PIN, INPUT_PULLUP);


    // 1. Configurazione WiFi
    for (int i = 0; i < loretoNetworksCount; i++) {
        wifiManager.addSSID(loretoNetworks[i].ssid, loretoNetworks[i].password);
    }

    wifiManager.setConnectionCallback(onConnectionChanged);
    wifiManager.init(8); // rssiGap di 8dB

    // 2. Lanciamo la prima scansione manuale
    wifiManager.startScan();
}

void loop() {
    // Aggiorna lo stato del WiFi (gestisce i risultati dello scan)
    wifiManager.update();
    // 2. Ottieni lo stato
    bool isNetReady = wifiManager.isConnected(); // dovrebbe essere lo stesso ritornato con canUseNetwork


    // --- TEST DISCONNESSIONE MANUALE ---
    // Se premi il pulsante (o colleghi il PIN 19 a GND)
    if (digitalRead(BUTTON_PIN) == LOW && isNetReady) {
        if (wifiManager.isConnected()) {
            wifiManager.disconnect();
            delay(500); // Debounce brutale per il test
        }
    }



    // --- LOGICA DEI SERVIZI ---
    if (isNetReady) {

        // Esegui Telegram solo se la rete è pronta
        // myTelegramBot.handleMessages();

        // Esegui Logica NTP ogni ora
        // if (now - lastNtpUpdate > 3600000) { ... }

    } else {

        // --- LOGICA DI RICONNESSIONE MANUALE ---
        // Se non siamo connessi, riproviamo a scansionare ogni 30s
        uint32_t now = millis();
        if (now - lastRetryTime > retryInterval) {
            lnLOG_NOTIFY("SISTEMA: Tentativo di riconnessione manuale...");
            wifiManager.startScan();
            lastRetryTime = now;
        }
    }





    // Altre attività che NON dipendono dal WiFi (es. sensori, LED)
    // readSensors();
}

