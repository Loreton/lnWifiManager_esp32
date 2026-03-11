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
    // m_credentialsCount = 0;
}

void lnWiFiManagerNB::init(int8_t rssiGap) {
    m_rssiGap = rssiGap;
    WiFi.mode(WIFI_STA);
    WiFi.persistent(false);
    WiFi.disconnect(true);
    WiFi.onEvent(WiFiEventHandler);
}


// #########################################
// # ....
// #########################################
void lnWiFiManagerNB::disconnect() {
    lnLOG_WARNING("%sForcing manual disconnect...", logPrefix);
    // true: spegne il modulo radio e cancella le credenziali temporanee
    // false: non cancella le credenziali dalla NVS (se presenti)
    WiFi.disconnect(true);
    delay(100);
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



/*
    Perché abbiamo usato s_instance?
    Questa è una delle domande più classiche quando si lavora con le classi in C++ su sistemi embedded.
    Il motivo è che WiFi.onEvent (il sistema che gestisce gli eventi dell'ESP32) si aspetta come argomento una funzione "semplice" o statica.

    Una funzione non statica (normale metodo di una classe) ha sempre un parametro "invisibile" chiamato this,
    che punta all'istanza specifica della classe.

    Il sistema degli eventi dell'ESP32 è globale e non sa quale istanza di lnWiFiManagerNB stai usando.
    Quindi non può passare il puntatore this.

    La soluzione:
        Rendiamo il gestore eventi (WiFiEventHandler) statico. Ora il sistema può chiamarlo.
        Però, essendo statico, il gestore non può vedere le variabili della tua istanza (come m_connCallback).
        Quindi salviamo l'indirizzo della classe in una variabile statica (s_instance) durante il costruttore.
        Quando scatta l'evento, il gestore statico "guarda" dentro s_instance per trovare e chiamare la tua callback specifica.
*/
void lnWiFiManagerNB::WiFiEventHandler(WiFiEvent_t event, WiFiEventInfo_t info) {
    if (s_instance == nullptr) return;
    const char* eventName;

    switch (event) {
        case ARDUINO_EVENT_WIFI_READY:            eventName = "WIFI_READY"; break;
        case ARDUINO_EVENT_WIFI_STA_START:        eventName = "STA_START"; break;
        case ARDUINO_EVENT_WIFI_STA_STOP:         eventName = "STA_STOP"; break;
        case ARDUINO_EVENT_WIFI_STA_CONNECTED:    eventName = "STA_CONNECTED"; break;
        case ARDUINO_EVENT_WIFI_STA_LOST_IP:      eventName = "STA_LOST_IP"; break;
        case ARDUINO_EVENT_WIFI_STA_GOT_IP:       eventName = "STA_GOT_IP"; break;
        case ARDUINO_EVENT_WIFI_STA_DISCONNECTED: eventName = "STA_DISCONNECTED"; break;
        case ARDUINO_EVENT_WIFI_SCAN_DONE:        eventName = "SCAN_DONE"; break;
        default:                                  eventName = "Unkown event name!"; break;
    }


    lnLOG_NOTIFY("%sWiFi Event: %s (%d)", logPrefix, eventName, (int)event);

    if (event == ARDUINO_EVENT_WIFI_STA_GOT_IP) {
        lnLOG_INFO("%sGot IP %s", logPrefix, WiFi.localIP().toString().c_str());
        lnLOG_INFO("%sGW     %s", logPrefix, WiFi.gatewayIP().toString().c_str());
        lnLOG_INFO("%sDNS    %s", logPrefix, WiFi.dnsIP().toString().c_str());
        lnLOG_INFO("%sRSSI   %d", logPrefix, WiFi.RSSI());
        s_instance->m_isNetworkActive = true; // ADESSO SIAMO DAVVERO ONLINE
        if (s_instance->m_connCallback) s_instance->m_connCallback(s_instance->m_isNetworkActive);
        // if (s_instance->m_connCallback) s_instance->m_connCallback(true);
    }

    else if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
        uint8_t reason = info.wifi_sta_disconnected.reason;
        const char* reasonStr;

        // Alcuni codici comuni (rif: Espressif esp_wifi_types.h)
        switch(reason) {
            case 1:  reasonStr = "UNSPECIFIED"; break;
            case 2:  reasonStr = "AUTH_EXPIRE"; break;
            case 3:  reasonStr = "AUTH_LEAVE (Manual Disconnect)"; break; // Hai chiamato disconnect() manualmente.
            case 8:  reasonStr = "ASSOC_LEAVE"; break;
            case 15: reasonStr = "4WAY_HANDSHAKE_TIMEOUT (Wrong Password?)"; break;
            case 201: reasonStr = "NO_AP_FOUND"; break;
            case 204: reasonStr = "HANDSHAKE_TIMEOUT (Segnale instabile/debole?)"; break;
            default: reasonStr = "OTHER"; break;
        }

        lnLOG_ERROR("%sDisconnected. Reason: %d (%s)", logPrefix, reason, reasonStr);
        s_instance->m_isNetworkActive = false; // RETE PERDUTA
        if (s_instance->m_connCallback) s_instance->m_connCallback(s_instance->m_isNetworkActive);
        // if (s_instance->m_connCallback) s_instance->m_connCallback(false);
    }

    else if (event == ARDUINO_EVENT_WIFI_STA_LOST_IP) {
        s_instance->m_isNetworkActive = false;
        if (s_instance->m_connCallback) s_instance->m_connCallback(s_instance->m_isNetworkActive);
        // if (s_instance->m_connCallback) s_instance->m_connCallback(false);
    }

    else if (event == ARDUINO_EVENT_WIFI_SCAN_DONE) {
        if (s_instance->m_scanCallback) s_instance->m_scanCallback(false); // scan terminato non siamo più in scan mode
    }

}

// void lnWiFiManagerNB::printScanResults() {
//     int n = WiFi.scanComplete();
//     lnLOG_DEBUG("%s--- Found %d networks ---", logPrefix, n);
//     for (int i = 0; i < n; ++i) {
//         bool saved = false;
//         for(uint8_t j=0; j<m_credentialsCount; j++) {
//             if(WiFi.SSID(i) == m_credentials[j].ssid) { saved = true; break; }
//         }
//         lnLOG_DEBUG("  %s %-20s RSSI: %d", saved ? "[*]" : "[ ]", WiFi.SSID(i).c_str(), WiFi.RSSI(i));
//     }
// }



void lnWiFiManagerNB::printScanResults() {
    int n = WiFi.scanComplete();
    if (n < 0) return;

    lnLOG_DEBUG("%s--- Found %d networks ---", logPrefix, n);
    for (int i = 0; i < n; ++i) {
        bool saved = false;
        // Verifichiamo se l'SSID è tra quelli salvati
        for(uint8_t j = 0; j < m_credentialsCount; j++) {
            if(WiFi.SSID(i) == m_credentials[j].ssid) {
                saved = true;
                break;
            }
        }

        // Recuperiamo il BSSID (MAC Address dell'AP)
        String bssid = WiFi.BSSIDstr(i);
        int rssi = WiFi.RSSI(i);
        String ssid = WiFi.SSID(i);

        // Stampa formattata: [ ] o [*] | SSID | BSSID | RSSI
        lnLOG_DEBUG("  %s %-20s [%s] RSSI: %d dBm",
                    saved ? "[*]" : "[ ]",
                    ssid.c_str(),
                    bssid.c_str(),
                    rssi);
    }
    lnLOG_DEBUG("%s-----------------------", logPrefix);
}

const char* lnWiFiManagerNB::getConnectedSSID() { return m_currentSSID; }