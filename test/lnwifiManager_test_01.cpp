//
// updated by ...: Loreto Notarantonio
// Date .........: 20-02-2026 17.54.06
//



#include <Arduino.h>

// #define LOG_MODULE_LEVEL LOG_MODULE_INFO
#include "lnLogger_Class.h"

// --- Project
#define  __I_AM_MAIN_CPP__
#include "lnWiFiManager.h"


// --- CREDENTIALS
#include <ssid_credentials_esp32.h>

//
lnWiFiManagerNB wifiManager;
bool canUseNetwork = false;
uint32_t lastRetryTime = 0;
const uint32_t retryInterval = 30000; // Riprova ogni 30 secondi se disconnesso


void onConnectionStatus(bool connected) {
    canUseNetwork = connected;
    if (connected) {
        lnLOG_INFO("SISTEMA: Rete OK. Ripristino Telegram/NTP...");
        // ntp.begin();
    } else {
        lnLOG_INFO("SISTEMA: Rete Persa. Sospensione servizi...");
    }
}



void setup() {
    Serial.begin(115200);
    lnLog.init(128, 20);  // line_buffer_len, filename_buffer_len

    wifiManager.setConnectionCallback(onConnectionStatus);

    // - prima dell'init()
    for (int8_t i = 0; i < loretoNetworksCount; i++) {
        wifiManager.addSSID(loretoNetworks[i].ssid, loretoNetworks[i].password);
    }

    wifiManager.init(8); // Timeout 5 min
    wifiManager.startScan(); // Primo avvio manuale
}



void loop() {
    wifiManager.update();

    if (canUseNetwork) {
        // --- QUI I TUOI SERVIZI ATTIVI ---
        // telegram.loop();
    } else {
        // --- LOGICA DI RETRY MANUALE ---
        uint32_t now = millis();
        if (now - lastRetryTime > retryInterval) {
            lnLOG_WARNING("SISTEMA: Tentativo di riconnessione manuale...");
            wifiManager.startScan();
            lastRetryTime = now;
        }
    }

    delay(10);
}