#include "lora.hpp"
#include <lmic.h>
#include <hal/hal.h>
#include "types.h"
#include "debug.hpp"
#include "config.h"

#ifdef __CONFIG_H__
static const PROGMEM u1_t NSKEY[16] = CONFIG_NSKEY;
static const PROGMEM u1_t ASKEY[16] = CONFIG_ASKEY;
static const PROGMEM u4_t DEVADDR = CONFIG_DEVADDR;
static const PROGMEM u4_t NETID = CONFIG_NETID;
#else
#error "LoRaWAN credentials are not defined"
#endif

const lmic_pinmap lmic_pins =
{
    .nss = 10,
    .rxtx = LMIC_UNUSED_PIN,
    .rst = 9,
    .dio = { 2, 6, 7 }
};

static osjob_t sendjob;

// These callbacks are only used in over-the-air activation, so they are
// left empty here (we cannot leave them out completely unless
// DISABLE_JOIN is set in config.h, otherwise the linker will complain).
void os_getArtEui(u1_t* buf) { }
void os_getDevEui(u1_t* buf) { }
void os_getDevKey(u1_t* buf) { }

extern collection_t measurementBuffer;
static void sendData(osjob_t* j)
{
    // Check if there is not a current TX/RX job running
    if(LMIC.opmode & OP_TXRXPEND)
    {
        DebugPrintLine(F("OP_TXRXPEND"));
    }
    else
    {
        DebugPrint(F("Sending "));
        DebugPrint(sizeof(measurementBuffer));
        DebugPrintLine(F("b..."));
        LMIC_setTxData2(CONFIG_CHANNEL, (uint8_t*)&measurementBuffer, sizeof(measurementBuffer), 0);
    }
}

void onEvent(ev_t ev)
{
    DebugPrint(os_getTime());
    DebugPrint(": ");
    switch(ev)
    {
        case EV_SCAN_TIMEOUT:
            DebugPrintLine(F("EV_SCAN_TIMEOUT"));
            break;
        case EV_BEACON_FOUND:
            DebugPrintLine(F("EV_BEACON_FOUND"));
            break;
        case EV_BEACON_MISSED:
            DebugPrintLine(F("EV_BEACON_MISSED"));
            break;
        case EV_BEACON_TRACKED:
            DebugPrintLine(F("EV_BEACON_TRACKED"));
            break;
        case EV_JOINING:
            DebugPrintLine(F("EV_JOINING"));
            break;
        case EV_JOINED:
            DebugPrintLine(F("EV_JOINED"));
            break;
        case EV_RFU1:
            DebugPrintLine(F("EV_RFU1"));
            break;
        case EV_JOIN_FAILED:
            DebugPrintLine(F("EV_JOIN_FAILED"));
            break;
        case EV_REJOIN_FAILED:
            DebugPrintLine(F("EV_REJOIN_FAILED"));
            break;
        case EV_TXCOMPLETE:
            DebugPrintLine(F("EV_TXCOMPLETE"));
            if(LMIC.txrxFlags & TXRX_ACK)
            {
                DebugPrintLine(F("TXRX_ACK"));
            }
            if(LMIC.dataLen > 0)
            {
                //uint8_t downlink[LMIC.dataLen];
                uint8_t downlink[64];
                memcpy(downlink, LMIC.frame + LMIC.dataBeg, LMIC.dataLen);
                DebugPrint(F("Data Received: "));
                DebugWrite(LMIC.frame + LMIC.dataBeg, LMIC.dataLen);
                DebugPrintLine();

            }

            os_setTimedCallback(&sendjob, os_getTime() + sec2osticks(CONFIG_INTERVAL), sendData);
            break;
        case EV_LOST_TSYNC:
            DebugPrintLine(F("EV_LOST_TSYNC"));
            break;
        case EV_RESET:
            DebugPrintLine(F("EV_RESET"));
            break;
        case EV_RXCOMPLETE:
            DebugPrintLine(F("EV_RXCOMPLETE"));
            break;
        case EV_LINK_DEAD:
            DebugPrintLine(F("EV_LINK_DEAD"));
            break;
        case EV_LINK_ALIVE:
            DebugPrintLine(F("EV_LINK_ALIVE"));
            break;
            /*case EV_SCAN_FOUND:
                DebugPrintLine(F("EV_SCAN_FOUND"));
                break;
            case EV_TXSTART:
                DebugPrintLine(F("EV_TXSTART"));
                break;
            case EV_TXCANCELED:
                DebugPrintLine(F("EV_TXCANCELED"));
                break;
            case EV_RXSTART:
                break;
            case EV_JOIN_TXCOMPLETE:
                DebugPrintLine(F("EV_JOIN_TXCOMPLETE"));
                break;*/
        default:
            DebugPrint(F("Unknown event: "));
            DebugPrintLine((unsigned int)ev);
            break;
    }
}

void setupLoRa(void)
{
    // LMIC init
    os_init();
    // Reset the MAC state. Session and pending data transfers will be discarded.
    LMIC_reset();

    // Set static session parameters. Instead of dynamically establishing a session
    // by joining the network, precomputed session parameters are be provided.
    #ifdef PROGMEM
    // On AVR, these values are stored in flash and only copied to RAM
    // once. Copy them to a temporary buffer here, LMIC_setSession will
    // copy them into a buffer of its own again.
    uint8_t nwkskey[sizeof(NSKEY)];
    uint8_t appskey[sizeof(ASKEY)];
    memcpy_P(nwkskey, NSKEY, sizeof(NSKEY));
    memcpy_P(appskey, ASKEY, sizeof(ASKEY));
    LMIC_setSession(NETID, DEVADDR, nwkskey, appskey);    // DIFF: LMIC_setSession(0x13, DEVADDR, nwkskey, appskey);
    #else
      // If not running an AVR with PROGMEM, just use the arrays directly
    LMIC_setSession(NETID, DEVADDR, NSKEY, ASKEY);    // DIFF: LMIC_setSession(0x13, DEVADDR, nwkskey, appskey);
    #endif

    #if defined(CFG_eu868)
    DebugPrintLine("European Channels");

    //LMIC_setupChannel(0, 868100000, DR_RANGE_MAP(DR_SF12, DR_SF7), BAND_CENTI);      // g-band
    //LMIC_setupChannel(1, 868300000, DR_RANGE_MAP(DR_SF12, DR_SF7B), BAND_CENTI);      // g-band
    //LMIC_setupChannel(2, 868500000, DR_RANGE_MAP(DR_SF12, DR_SF7), BAND_CENTI);      // g-band
    //LMIC_setupChannel(3, 867100000, DR_RANGE_MAP(DR_SF12, DR_SF7), BAND_CENTI);      // g-band
    //LMIC_setupChannel(4, 867300000, DR_RANGE_MAP(DR_SF12, DR_SF7), BAND_CENTI);      // g-band
    //LMIC_setupChannel(5, 867500000, DR_RANGE_MAP(DR_SF12, DR_SF7), BAND_CENTI);      // g-band
    //LMIC_setupChannel(6, 867700000, DR_RANGE_MAP(DR_SF12, DR_SF7), BAND_CENTI);      // g-band
    //LMIC_setupChannel(7, 867900000, DR_RANGE_MAP(DR_SF12, DR_SF7), BAND_CENTI);      // g-band
    //LMIC_setupChannel(8, 868800000, DR_RANGE_MAP(DR_FSK, DR_FSK), BAND_MILLI);      // g2-band

    /*for(int i = 1; i <= 8; i++)
    {
        LMIC_disableChannel(i);
    }*/
    #elif defined(CFG_us915)
    // NA-US channels 0-71 are configured automatically
    // but only one group of 8 should (a subband) should be active
    // TTN recommends the second sub band, 1 in a zero based count.
    // https://github.com/TheThingsNetwork/gateway-conf/blob/master/US-global_conf.json
    LMIC_selectSubBand(1);
    #endif

    // Disable link check validation
    LMIC_setLinkCheckMode(0);

    // TTN uses SF9 for its RX2 window.
    LMIC.dn2Dr = DR_SF9;

    // Set data rate and transmit power for uplink (note: txpow seems to be ignored by the library)
    LMIC_setDrTxpow(DR_SF7, 14);
    //LMIC_setDrTxpow(DR_SF12, 14);

    // Start job
    //sendData(&sendjob);
    os_setTimedCallback(&sendjob, os_getTime()/* + sec2osticks(CONFIG_INTERVAL)*/, sendData);
}
