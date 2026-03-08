//
// updated by ...: Loreto Notarantonio
// Date .........: 20-02-2026 17.16.03
//


// #include "lnLogger_LEVELS.h"
// #define LOG_MODULE_LEVEL LOG_LEVEL_DEBUG
#include "lnLogger_Class.h"

#include "lnWiFiManager.h"
const char* logPrefix="WiFi: ";


ScanCallback scanCallback = nullptr;

lnWiFiManagerNB* lnWiFiManagerNB::s_instance = nullptr;

lnWiFiManagerNB::lnWiFiManagerNB() {
    s_instance = this;
}


void lnWiFiManagerNB::setScanCallback(ScanCallback cb) {
    scanCallback = cb;
}

// #######################################################################################################
// # WiFi.persistent(false):
// #    Inserito in init(). Senza questo, ogni volta che chiami WiFi.begin(),
// #    l'ESP32 scrive le credenziali nella memoria Flash (NVS). La Flash ha cicli di scrittura limitati;
// #    disabilitandolo preservi il chip.
// #######################################################################################################
// void lnWiFiManagerNB::init(uint32_t scanIntervalWhenConnected, uint32_t scanIntervalWhenNotConnected, uint32_t maxWifiTimeout, int rssiGap) {
//     m_scanIntervalWhenConnected = scanIntervalWhenConnected;
//     m_scanIntervalWhenNotConnected = scanIntervalWhenNotConnected;
//     m_maxWifiTimeout = maxWifiTimeout;
//     m_rssiGap = rssiGap;

void lnWiFiManagerNB::init(uint16_t scanIntervalWhenConnected, uint16_t scanIntervalWhenNotConnected, uint16_t maxWifiTimeout, int8_t rssiGap) {
    m_scanIntervalWhenConnected = scanIntervalWhenConnected*1000;
    m_scanIntervalWhenNotConnected = scanIntervalWhenNotConnected*1000;
    m_maxWifiTimeout = maxWifiTimeout*1000;
    m_rssiGap = rssiGap;

    WiFi.mode(WIFI_STA);
    WiFi.persistent(false); // [Punto 5] Evita usura Flash
    WiFi.disconnect(true);

    WiFi.onEvent(WiFiEventHandler);

    m_lastConnectedTime = millis(); // [Punto D] Inizializzazione corretta
    startScan();
    // Serial.printf("LOG_MODULE_LEVEL: %d", LOG_MODULE_LEVEL);
    // Serial.printf("LOG_LEVEL_INFO: %d", LOG_LEVEL_INFO);
}

void lnWiFiManagerNB::addSSID(const char* ssid, const char* password) {
    WifiCredential cred;
    strncpy(cred.ssid, ssid, MAX_SSID_LENGTH - 1);
    strncpy(cred.password, password, MAX_PASS_LENGTH - 1);
    m_credentials.push_back(cred);
}



void lnWiFiManagerNB::update() {
    uint32_t now = millis();
    wl_status_t status = WiFi.status();

    // Se non siamo connessi, verifichiamo il timeout totale (Punto D)
    if (status != WL_CONNECTED) {
        if (now - m_lastConnectedTime > m_maxWifiTimeout) {
            lnLOG_WARNING("%sMax timeout reached. Forcing new scan.", logPrefix);
            m_lastConnectedTime = now;
            startScan();
            return;
        }
    } else {
        m_lastConnectedTime = now;
    }

    // --- LOGICA TIMEOUT CONNESSIONE ---
    // Se abbiamo appena lanciato un WiFi.begin(), aspettiamo 10s prima di scansionare ancora
    if (status != WL_CONNECTED && m_connectionStartTime > 0) {
        if (now - m_connectionStartTime < 10000) {
            return; // Troppo presto, attendi che il tentativo finisca
        }
    }

    // Timer scansione periodica
    uint32_t interval = (status == WL_CONNECTED) ? m_scanIntervalWhenConnected : m_scanIntervalWhenNotConnected;
    if (now - m_lastScanTime >= interval) {
        startScan();
    }

    if (WiFi.scanComplete() >= 0) {
        handleScanResult();
        WiFi.scanDelete();
    }
}



// ##################################################################################################################
// # Il timer m_lastScanTime viene aggiornato solo quando la scansione viene effettivamente lanciata.
// ##################################################################################################################
void lnWiFiManagerNB::startScan() {

    // [Punto A] Avvia la scansione solo se non ce n'è una in corso
    if (WiFi.scanComplete() == -2) {

        lnLOG_NOTIFY("%sStarting async scan...", logPrefix);
        if (scanCallback)
            scanCallback(true); // avvisa la CB che è partito lo scan
        WiFi.scanNetworks(true);
        m_lastScanTime = millis(); // Aggiorna il timer qui
    }
}



void lnWiFiManagerNB::handleScanResult() {
    int n = WiFi.scanComplete();
    if (n <= 0) return;

    // DEBUG: Mostra cosa abbiamo trovato
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

    lnLOG_DEBUG("%sbestIdx=%d", logPrefix, bestIdx);
    if (bestIdx == -1) return;

    if (WiFi.status() == WL_CONNECTED) {
        int currentRSSI = WiFi.RSSI();

        // --- FIX SICUREZZA ---
        uint8_t* currentBSSID = WiFi.BSSID();
        uint8_t* targetBSSID = WiFi.BSSID(bestIdx);

        // Se uno dei due è nullo, non possiamo confrontarli in sicurezza
        if (currentBSSID != nullptr && targetBSSID != nullptr) {
            if (memcmp(targetBSSID, currentBSSID, 6) == 0) {
                // Siamo già sull'AP migliore di questa rete
                return;
            }
        }

        // Controllo Gap
        if ((bestRSSI - currentRSSI) < m_rssiGap) {
            return;
        }

        // Serial.printf("WiFi: Switching to better AP (%s) RSSI: %d (Gap: %d)\n",
        //               WiFi.SSID(bestIdx).c_str(), bestRSSI, bestRSSI - currentRSSI);
        lnLOG_INFO("%sSwitching to better AP (%s) RSSI: %d (Gap: %d)", logPrefix,
                      WiFi.SSID(bestIdx).c_str(), bestRSSI, bestRSSI - currentRSSI);
    }

    // Aggiorna e connetti
    strncpy(m_currentSSID, WiFi.SSID(bestIdx).c_str(), MAX_SSID_LEN - 1);
    lnLOG_INFO("%sTrying connection to: ...%s", logPrefix, m_currentSSID);
    WiFi.begin(m_currentSSID, bestPassword);
}




void lnWiFiManagerNB::WiFiEventHandler(WiFiEvent_t event, WiFiEventInfo_t info) {

    if (s_instance != nullptr) {
        // Inoltra l'evento alla funzione di istanza (se vuoi gestirli lì)
        // s_instance->onWiFiEvent(event);

        // Oppure gestisci direttamente qui i log comuni:
        const char* eventName = "UNKNOWN";
        switch (event) {
            case ARDUINO_EVENT_WIFI_READY:           eventName = "WIFI_READY"; break;
            case ARDUINO_EVENT_WIFI_SCAN_DONE:       eventName = "SCAN_DONE"; break;
            case ARDUINO_EVENT_WIFI_STA_START:       eventName = "STA_START"; break;
            case ARDUINO_EVENT_WIFI_STA_STOP:        eventName = "STA_STOP"; break;
            case ARDUINO_EVENT_WIFI_STA_CONNECTED:   eventName = "STA_CONNECTED"; break;
            case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:eventName = "STA_DISCONNECTED"; break;
            case ARDUINO_EVENT_WIFI_STA_GOT_IP:      eventName = "STA_GOT_IP"; break;
            case ARDUINO_EVENT_WIFI_STA_LOST_IP:     eventName = "STA_LOST_IP"; break;
            default: break;
        }

        lnLOG_NOTIFY("%sEvent: %s (%d)", logPrefix, eventName, (int)event);
        if (event == ARDUINO_EVENT_WIFI_STA_GOT_IP) {
            // m_ipIsActive = true;
            lnLOG_INFO("%sGot IP %s", logPrefix, WiFi.localIP().toString().c_str());
            lnLOG_INFO("%sGW     %s", logPrefix, WiFi.gatewayIP().toString().c_str());
            lnLOG_INFO("%sDNS    %s", logPrefix, WiFi.dnsIP().toString().c_str());
            lnLOG_INFO("%sRSSI   %d", logPrefix, WiFi.RSSI());
        }

        else if (event == ARDUINO_EVENT_WIFI_STA_CONNECTED) {
            // m_ipIsActive = false;
            lnLOG_DEBUG("%sIP   %s", logPrefix, WiFi.localIP().toString().c_str());
            lnLOG_DEBUG("%sGW   %s", logPrefix, WiFi.gatewayIP().toString().c_str());
            lnLOG_DEBUG("%sDNS  %s", logPrefix, WiFi.dnsIP().toString().c_str());
            lnLOG_DEBUG("%sRSSI %d", logPrefix, WiFi.RSSI());
        }

        else if (event == ARDUINO_EVENT_WIFI_STA_LOST_IP) {
            // m_ipIsActive = false;
        }

        else if (event == ARDUINO_EVENT_WIFI_SCAN_DONE) {
            lnLOG_DEBUG("%sscan completed", logPrefix);
            if (scanCallback)
                scanCallback(false);   // scan finito

        }
    }
}


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

bool lnWiFiManagerNB::isConnected() { return WiFi.status() == WL_CONNECTED; }
const char* lnWiFiManagerNB::getConnectedSSID() { return m_currentSSID; }