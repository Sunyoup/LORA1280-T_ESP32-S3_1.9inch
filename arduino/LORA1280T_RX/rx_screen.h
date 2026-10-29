// Receiver screen: packet number, RSSI/SNR bars, message, statistics.
// Shared by the RX and RX_Interrupt sketches (same file in both folders).
// Needs gfx, lora_ui.h and the LORA_* settings defined before inclusion.
#pragma once

// Screen layout (y positions)
#define Y_CONFIG    34
#define Y_PKT_LBL   58
#define Y_PKT       70
#define Y_RSSI_LBL  108
#define Y_RSSI      120
#define Y_SNR_LBL   146
#define Y_SNR       158
#define Y_MSG_LBL   184
#define Y_MSG       196
#define Y_STAT_LBL  222
#define Y_STAT      234
#define Y_LOST      244
#define Y_FERR      254
#define Y_LAST      272

// Statistics
static uint32_t rxOk = 0, rxCrc = 0, rxOther = 0, rxLost = 0;
static long rxLastSeq = -1;
static uint32_t rxLastMs = 0;

void rxDrawStaticScreen(const char *title)
{
    gfx->fillScreen(UI_BG);
    gfx->setTextWrap(false);
    uiHeader(title, RGB565_CYAN);
    uiConfig(Y_CONFIG, LORA_FREQ, LORA_BW, LORA_SF, LORA_CR, "");
    uiLabel(Y_PKT_LBL,  "PACKET");
    uiLabel(Y_RSSI_LBL, "RSSI");
    uiLabel(Y_SNR_LBL,  "SNR");
    uiLabel(Y_MSG_LBL,  "MESSAGE");
    uiLabel(Y_STAT_LBL, "STATISTICS");
    uiText(4, Y_PKT, 4, UI_LABEL, "---", 7);
    uiText(4, Y_LAST, 2, UI_WARN, "Waiting...", 14);
}

void rxShowInitError(int state)
{
    char buf[16];
    snprintf(buf, sizeof(buf), "Init %d", state);
    uiText(4, Y_LAST, 2, UI_BAD, buf, 14);
}

// Packet number = the number after the last '#' in the message, or -1.
static long rxParseSeq(const String &msg)
{
    int i = msg.lastIndexOf('#');
    if (i < 0 || i + 1 >= (int)msg.length() || !isDigit(msg[i + 1])) return -1;
    return msg.substring(i + 1).toInt();
}

static void rxDrawStats()
{
    char buf[32];
    snprintf(buf, sizeof(buf), "OK %lu  CRC %lu  Err %lu",
             (unsigned long)rxOk, (unsigned long)rxCrc, (unsigned long)rxOther);
    uiText(4, Y_STAT, 1, UI_TEXT, buf, 27);
    uint32_t total = rxOk + rxLost;
    snprintf(buf, sizeof(buf), "Lost %lu  (%.1f%%)",
             (unsigned long)rxLost, total ? 100.0f * rxLost / total : 0.0f);
    uiText(4, Y_LOST, 1, rxLost ? UI_WARN : UI_TEXT, buf, 27);
}

void rxShowPacket(const String &msg, float rssi, float snr, float ferr)
{
    rxOk++;
    rxLastMs = millis();

    // Count gaps in the packet numbers as lost packets.
    // A smaller number means the transmitter restarted: just resync.
    long seq = rxParseSeq(msg);
    if (seq >= 0 && rxLastSeq >= 0 && seq > rxLastSeq + 1) {
        rxLost += seq - rxLastSeq - 1;
    }
    if (seq >= 0) rxLastSeq = seq;

    char buf[32];
    if (seq >= 0) snprintf(buf, sizeof(buf), "#%ld", seq);
    else          snprintf(buf, sizeof(buf), "%lu", (unsigned long)rxOk);
    uiText(4, Y_PKT, 4, UI_TEXT, buf, 7);

    // RSSI: -110 dBm (weak) .. -30 dBm (strong)
    uint16_t c = rssi > -80 ? UI_GOOD : (rssi > -100 ? UI_WARN : UI_BAD);
    snprintf(buf, sizeof(buf), "%.0f dBm", rssi);
    uiText(4, Y_RSSI, 2, c, buf, 8);
    uiBar(104, Y_RSSI + 2, 62, 12, rssi, -110, -30, c);

    // SNR: LoRa still decodes below 0 dB; -10 .. +15 dB
    c = snr > 5 ? UI_GOOD : (snr > -5 ? UI_WARN : UI_BAD);
    snprintf(buf, sizeof(buf), "%.1f dB", snr);
    uiText(4, Y_SNR, 2, c, buf, 8);
    uiBar(104, Y_SNR + 2, 62, 12, snr, -10, 15, c);

    uiMessage(Y_MSG, msg);
    rxDrawStats();

    snprintf(buf, sizeof(buf), "Freq err %+.0f Hz", ferr);
    uiText(4, Y_FERR, 1, UI_LABEL, buf, 27);
}

void rxShowError(int state)
{
    if (state == RADIOLIB_ERR_CRC_MISMATCH) rxCrc++;
    else rxOther++;
    rxDrawStats();
}

// "Last RX" age and uptime; call often, redraws at most every 250 ms.
void rxUpdateClock()
{
    static uint32_t lastDraw = 0;
    uint32_t now = millis();
    if (now - lastDraw < 250) return;
    lastDraw = now;

    if (rxOk > 0) {
        float age = (now - rxLastMs) / 1000.0f;
        char buf[20];
        if (age < 100) snprintf(buf, sizeof(buf), "Last %.1fs", age);
        else           snprintf(buf, sizeof(buf), "Last %.0fs", age);
        uiText(4, Y_LAST, 2, age < 3 ? UI_GOOD : UI_BAD, buf, 14);
    }
    uiUptime();
}
