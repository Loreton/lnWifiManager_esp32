//
// updated by ...: Loreto Notarantonio
// Date .........: 13-09-2025 17.32.34
//


#pragma once

// Ho sostituito String con array di char e aggiunto la gestione del BSSID per il roaming reale tra AP con lo stesso SSID.

#include <Arduino.h>
#include <WiFi.h>
#include <vector>

#ifndef MAX_SSID_LENGTH
    #define MAX_SSID_LENGTH 33 // MAX_SSID_LEN è già definita altrove
    #define MAX_PASS_LENGTH 64
#endif

// Callback per lo stato della scansione
typedef void (*ScanCallback)(bool scanning);
// Callback per lo stato della connessione (true = connesso/IP, false = disconnesso)
typedef void (*ConnectionStatusCallback)(bool connected);

class lnWiFiManagerNB {
    public:
        lnWiFiManagerNB();

        // Inizializzazione (i parametri di intervallo ora servono solo come riferimento o timeout)
        void init(uint16_t maxWifiTimeoutSeconds);

        void update();
        void addSSID(const char* ssid, const char* password);

        // Comandi Manuali
        void startScan();

        // Getters e Callbacks
        bool isConnected();
        const char* getConnectedSSID();
        void setScanCallback(ScanCallback cb);
        void setConnectionCallback(ConnectionStatusCallback cb);

        void printScanResults();

    private:
        static lnWiFiManagerNB* s_instance;
        static void WiFiEventHandler(WiFiEvent_t event, WiFiEventInfo_t info);

        struct WifiCredential {
            char ssid[MAX_SSID_LENGTH];
            char password[MAX_PASS_LENGTH];
        };

        std::vector<WifiCredential> m_credentials;

        ConnectionStatusCallback m_connCallback = nullptr;
        ScanCallback m_scanCallback = nullptr;

        uint32_t m_maxWifiTimeout;
        uint32_t m_lastConnectedTime = 0;
        char     m_currentSSID[MAX_SSID_LENGTH] = {0};

        void handleScanResult();
};