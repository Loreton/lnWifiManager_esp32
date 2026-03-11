//
// updated by ...: Loreto Notarantonio
// Date .........: 13-09-2025 17.32.34
//

#pragma once

#include <Arduino.h>
#include <WiFi.h>

#define MAX_SSID_LENGTH 33
#define MAX_PASS_LENGTH 64
#define MAX_STORED_NETWORKS 10  // Numero massimo di reti memorizzabili

typedef void (*ScanCallback)(bool scanning);
typedef void (*ConnectionStatusCallback)(bool connected);

class lnWiFiManagerNB {
    public:
        lnWiFiManagerNB();

        void init(int8_t rssiGap = 5);
        void update();
        void addSSID(const char* ssid, const char* password);
        void startScan();

        // bool isConnected();
        // Ora isConnected non interroga più il driver WiFi, ma legge il nostro stato "validato"
        bool isConnected() { return m_isNetworkActive; }
        const char* getConnectedSSID();
        void setScanCallback(ScanCallback cb);
        void setConnectionCallback(ConnectionStatusCallback cb);
        void printScanResults();
        void disconnect();



    private:
        static lnWiFiManagerNB* s_instance;
        static void WiFiEventHandler(WiFiEvent_t event, WiFiEventInfo_t info);

        struct WifiCredential {
            char ssid[MAX_SSID_LENGTH];
            char password[MAX_PASS_LENGTH];
        };

        // Sostituito vector con array fisso
        WifiCredential m_credentials[MAX_STORED_NETWORKS];
        uint8_t m_credentialsCount = 0;

        ConnectionStatusCallback m_connCallback = nullptr;
        ScanCallback m_scanCallback = nullptr;

        int8_t   m_rssiGap;
        char     m_currentSSID[MAX_SSID_LENGTH] = {0};
        bool     m_isNetworkActive = false; // La nostra "Sorgente di Verità"

        void handleScanResult();
};