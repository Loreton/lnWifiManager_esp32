//
// updated by ...: Loreto Notarantonio
// Date .........: 20-02-2026 17.16.03
//


#include "lnWiFiManager.h"
#include "lnLogger_Class.h"

const char* logPrefix = "WiFi: ";
lnWiFiManagerNB* lnWiFiManagerNB::s_instance = nullptr;

lnWiFiManagerNB::lnWiFiManagerNB() {
    s_instance = this;
}

void lnWiFiManagerNB::init(uint16_t maxWifiTimeoutSeconds) {
    m_maxWifiTimeout = maxWifiTimeoutSeconds * 1000;

    WiFi.mode(WIFI_STA);
    WiFi.persistent(false);
    WiFi.disconnect(true);

    WiFi.onEvent(WiFiEventHandler);
    m_lastConnectedTime = millis();

    lnLOG_INFO("%sInitialized in manual mode.", logPrefix);
}

void lnWiFiManagerNB::setScanCallback(ScanCallback cb) { m_scanCallback = cb; }
void lnWiFiManagerNB::setConnectionCallback(ConnectionStatusCallback cb) { m_connCallback = cb; }

void lnWiFiManagerNB::addSSID(const char* ssid, const char* password) {
    WifiCredential cred;
    strncpy(cred.ssid, ssid, MAX_SSID_LENGTH - 1);
    strncpy(cred.password, password, MAX_PASS_LENGTH - 1);
    m_credentials.push_back(cred);
}

void lnWiFiManagerNB::startScan() {
    // Avvia la scansione solo se non ce n'è già una in corso
    if (WiFi.scanComplete() == -2) {
        lnLOG_NOTIFY("%sStarting manual async scan...", logPrefix);
        if (m_scanCallback) m_scanCallback(true);
        WiFi.scanNetworks(true); // true = async
    }
}

void lnWiFiManagerNB::update() {
    uint32_t now = millis();

    if (WiFi.status() == WL_CONNECTED) {
        m_lastConnectedTime = now;
    }

    // Gestione del risultato della scansione (se avviata manualmente)
    int n = WiFi.scanComplete();
    if (n >= 0) {
        handleScanResult();
        WiFi.scanDelete();
    }
}

void lnWiFiManagerNB::handleScanResult() {
    int n = WiFi.scanComplete();
    if (n <= 0) return;

    printScanResults();

    int bestRSSI = -1000;
    int bestIdx = -1;
    const char* bestPassword = nullptr;

    for (int i = 0; i < n; ++i) {
        for (auto &cred : m_credentials) {
            if (strcmp(WiFi.SSID(i).c_str(), cred.ssid) == 0) {
                if (WiFi.RSSI(i) > bestRSSI) {
                    bestRSSI = WiFi.RSSI(i);
                    bestIdx = i;
                    bestPassword = cred.password;
                }
            }
        }
    }

    if (bestIdx != -1) {
        strncpy(m_currentSSID, WiFi.SSID(bestIdx).c_str(), MAX_SSID_LENGTH - 1);
        lnLOG_INFO("%sConnecting to: %s", logPrefix, m_currentSSID);
        WiFi.begin(m_currentSSID, bestPassword);
    }
}

void lnWiFiManagerNB::WiFiEventHandler(WiFiEvent_t event, WiFiEventInfo_t info) {
    if (s_instance == nullptr) return;

    switch (event) {
        case ARDUINO_EVENT_WIFI_STA_GOT_IP:
            lnLOG_INFO("%sEvent: GOT_IP (%s)", logPrefix, WiFi.localIP().toString().c_str());
            if (s_instance->m_connCallback) s_instance->m_connCallback(true);
            break;

        case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
            lnLOG_WARNING("%sEvent: DISCONNECTED", logPrefix);
            if (s_instance->m_connCallback) s_instance->m_connCallback(false);
            break;

        case ARDUINO_EVENT_WIFI_SCAN_DONE:
            lnLOG_DEBUG("%sEvent: SCAN_DONE", logPrefix);
            if (s_instance->m_scanCallback) s_instance->m_scanCallback(false);
            break;

        default: break;
    }
}

void lnWiFiManagerNB::printScanResults() {
    int n = WiFi.scanComplete();
    if (n < 0) return;
    lnLOG_DEBUG("%s--- Scan Results ---", logPrefix);
    for (int i = 0; i < n; ++i) {
        lnLOG_DEBUG("%sSSID: %-20s RSSI: %d", logPrefix, WiFi.SSID(i).c_str(), WiFi.RSSI(i));
    }
}

/*
// Questa versione confronta ogni rete trovata con quelle nella tua lista m_credentials. S
// e c'è un match, aggiunge un indicatore visivo.
void lnWiFiManagerNB::printScanResults() {
    int n = WiFi.scanComplete();
    if (n < 0) return;

    // Serial.println("\n--- WiFi Scan Results ---");
    lnLOG_DEBUG("%s--- WiFi Scan Results ---", logPrefix);
    for (int i = 0; i < n; ++i) {
        bool isSaved = false;
        for (auto &cred : m_credentials) {
            if (WiFi.SSID(i) == cred.ssid) {
                isSaved = true;
                break;
            }
        }

        // Serial.printf("%s %-20s RSSI: %d dBm %s\n",
        lnLOG_DEBUG("%s%s %-20s RSSI: %d dBm %s", logPrefix,
            isSaved ? "[*]" : "[ ]",      // Asterisco se la rete è salvata
            WiFi.SSID(i).c_str(),
            WiFi.RSSI(i),
            (WiFi.status() == WL_CONNECTED && WiFi.SSID(i) == WiFi.SSID()) ? "<-- ACTIVE" : ""
        );
    }
    // Serial.println("--------------------------\n");
    lnLOG_INFO("%s--------------------------", logPrefix);
}
*/
bool lnWiFiManagerNB::isConnected() { return WiFi.status() == WL_CONNECTED; }
const char* lnWiFiManagerNB::getConnectedSSID() { return m_currentSSID; }