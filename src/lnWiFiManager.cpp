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
    m_credentialsCount = 0;
}

void lnWiFiManagerNB::init(int8_t rssiGap) {
    m_rssiGap = rssiGap;
    WiFi.mode(WIFI_STA);
    WiFi.persistent(false);
    WiFi.disconnect(true);
    WiFi.onEvent(WiFiEventHandler);
}

void lnWiFiManagerNB::setScanCallback(ScanCallback cb) {
    m_scanCallback = cb;
}

void lnWiFiManagerNB::setConnectionCallback(ConnectionStatusCallback cb) {
    m_connCallback = cb;
}

void lnWiFiManagerNB::addSSID(const char* ssid, const char* password) {
    if (m_credentialsCount < MAX_STORED_NETWORKS) {
        strncpy(m_credentials[m_credentialsCount].ssid, ssid, MAX_SSID_LENGTH - 1);
        strncpy(m_credentials[m_credentialsCount].password, password, MAX_PASS_LENGTH - 1);
        m_credentialsCount++;
    } else {
        lnLOG_ERROR("%sMax credentials reached! Cannot add %s", logPrefix, ssid);
    }
}

void lnWiFiManagerNB::startScan() {
    if (WiFi.scanComplete() == -2) {
        lnLOG_NOTIFY("%sStarting manual scan...", logPrefix);
        if (m_scanCallback) m_scanCallback(true);
        WiFi.scanNetworks(true);
    }
}

void lnWiFiManagerNB::update() {
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

    // Ciclo sulle reti trovate dallo scan
    for (int i = 0; i < n; ++i) {
        // Ciclo sull'array fisso delle credenziali
        for (uint8_t j = 0; j < m_credentialsCount; ++j) {
            if (strcmp(WiFi.SSID(i).c_str(), m_credentials[j].ssid) == 0) {
                int currentRSSI = WiFi.RSSI(i);
                if (currentRSSI > bestRSSI) {
                    bestRSSI = currentRSSI;
                    bestIdx = i;
                    bestPassword = m_credentials[j].password;
                }
            }
        }
    }

    if (bestIdx != -1) {
        if (WiFi.status() == WL_CONNECTED) {
            if (strcmp(WiFi.SSID().c_str(), WiFi.SSID(bestIdx).c_str()) == 0) return;
            if ((bestRSSI - WiFi.RSSI()) < m_rssiGap) return;
        }

        strncpy(m_currentSSID, WiFi.SSID(bestIdx).c_str(), MAX_SSID_LENGTH - 1);
        lnLOG_INFO("%sConnecting to: %s (%d dBm)", logPrefix, m_currentSSID, bestRSSI);
        WiFi.begin(m_currentSSID, bestPassword);
    }
}

void lnWiFiManagerNB::WiFiEventHandler(WiFiEvent_t event, WiFiEventInfo_t info) {
    if (s_instance == nullptr) return;
    const char* eventName;

    switch (event) {
        /*
        */
        case ARDUINO_EVENT_WIFI_READY:           eventName = "WIFI_READY"; break;
        case ARDUINO_EVENT_WIFI_STA_START:       eventName = "STA_START"; break;
        case ARDUINO_EVENT_WIFI_STA_STOP:        eventName = "STA_STOP"; break;
        case ARDUINO_EVENT_WIFI_STA_CONNECTED:   eventName = "STA_CONNECTED"; break;
        case ARDUINO_EVENT_WIFI_STA_LOST_IP:     eventName = "STA_LOST_IP"; break;

        case ARDUINO_EVENT_WIFI_STA_GOT_IP:
            eventName = "STA_GOT_IP";
            lnLOG_INFO("%sSTA_GOT_IP: %s", logPrefix, WiFi.localIP().toString().c_str());
            if (s_instance->m_connCallback) s_instance->m_connCallback(true);
            break;

        case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
            eventName = "STA_DISCONNECTED";
            lnLOG_INFO("%sSTA_DISCONNECTED", logPrefix);
            if (s_instance->m_connCallback) s_instance->m_connCallback(false);
            break;

        case ARDUINO_EVENT_WIFI_SCAN_DONE:
            eventName = "SCAN_DONE";
            lnLOG_INFO("%sSCAN_DONE", logPrefix);
            if (s_instance->m_scanCallback) s_instance->m_scanCallback(false);
            break;

        default:
            eventName = "Unkown event name!";
            break;
    }
    lnLOG_NOTIFY("%sWiFi Event: %s (%d)", logPrefix, eventName, (int)event);

}

void lnWiFiManagerNB::printScanResults() {
    int n = WiFi.scanComplete();
    lnLOG_DEBUG("%s--- Found %d networks ---", logPrefix, n);
    for (int i = 0; i < n; ++i) {
        bool saved = false;
        for(uint8_t j=0; j<m_credentialsCount; j++) {
            if(WiFi.SSID(i) == m_credentials[j].ssid) { saved = true; break; }
        }
        lnLOG_DEBUG("  %s %-20s RSSI: %d", saved ? "[*]" : "[ ]", WiFi.SSID(i).c_str(), WiFi.RSSI(i));
    }
}

bool lnWiFiManagerNB::isConnected() { return WiFi.status() == WL_CONNECTED; }
const char* lnWiFiManagerNB::getConnectedSSID() { return m_currentSSID; }