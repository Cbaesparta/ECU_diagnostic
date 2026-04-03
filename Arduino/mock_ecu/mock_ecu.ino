/*  mock_ecu.ino — Arduino Mock ECU
 *
 *  Simulates a vehicle ECU by sending periodic CAN frames over a
 *  MCP2515 module + TJA1050 transceiver at 1 Mbps (standard 11-bit frames).
 *
 *  Wiring (Arduino Uno/Nano):
 *  +----------------+------------------------------------------+
 *  | Arduino Pin    | MCP2515 Module Pin                       |
 *  +----------------+------------------------------------------+
 *  | D10  (SS)      | CS                                       |
 *  | D11  (MOSI)    | SI                                       |
 *  | D12  (MISO)    | SO                                       |
 *  | D13  (SCK)     | SCK                                      |
 *  | D2   (INT)     | INT                                      |
 *  | 5V             | VCC (if module is 5V; check your board)  |
 *  | GND            | GND                                      |
 *  +----------------+------------------------------------------+
 *
 *  MCP2515 → TJA1050 → CAN bus (H/L pair, 120 Ω termination at each end)
 *                     → TJA1050 → STM32F429 PA11/PA12 (CAN1)
 *
 *  Required library: coryjfowler/MCP_CAN_lib  (Arduino Library Manager:
 *                    search "MCP_CAN")
 *
 *  Crystal on MCP2515 module:
 *    Change MCP_CRYSTAL below to MCP_8MHZ if your module has an 8 MHz crystal.
 *    Most cheap blue modules have 8 MHz; the silver/black shielded ones have 16 MHz.
 *    Check the oscillator can next to the MCP2515 chip.
 *
 *  CAN IDs (must match can_diagnostic.h on STM32):
 *    0x100  RPM            2 bytes big-endian uint16  (0–8000)
 *    0x101  Speed          1 byte  uint8  (km/h, 0–255)
 *    0x102  Engine Temp    1 byte  uint8  (°C + 40 offset, raw 0–255 → -40..215 °C)
 *    0x103  Battery        2 bytes big-endian uint16  (mV, e.g. 12500 = 12.50 V)
 *    0x104  Throttle       1 byte  uint8  (%, 0–100)
 */

#include <SPI.h>
#include <mcp_can.h>

/* ---- Configuration --------------------------------------------------- */
#define MCP_CS_PIN     10       /* SPI chip-select */
#define MCP_INT_PIN    2        /* MCP2515 interrupt pin */
#define MCP_CRYSTAL    MCP_16MHZ  /* Change to MCP_8MHZ if needed */
#define CAN_SPEED      CAN_1000KBPS

/* Transmission interval per PID (ms) */
#define TX_INTERVAL_MS  100U

/* ---- CAN message IDs (must match STM32 firmware) --------------------- */
#define CAN_ID_RPM          0x100
#define CAN_ID_SPEED        0x101
#define CAN_ID_ENGINE_TEMP  0x102
#define CAN_ID_BATTERY      0x103
#define CAN_ID_THROTTLE     0x104

MCP_CAN CAN(MCP_CS_PIN);

/* ---- Simulated sensor state ----------------------------------------- */
struct SimState {
    float   rpm;          /* current RPM */
    float   rpm_target;   /* target (interpolated) */
    float   speed;
    float   eng_temp;
    float   battery;
    float   throttle;
    uint32_t last_tx;
    uint8_t  pid_index;   /* round-robin PID selector */
} sim = {
    .rpm        = 800.0f,
    .rpm_target = 800.0f,
    .speed      = 0.0f,
    .eng_temp   = 20.0f,
    .battery    = 12400.0f,
    .throttle   = 0.0f,
    .last_tx    = 0,
    .pid_index  = 0
};

/* ======================================================================
 * setup
 * ==================================================================== */
void setup()
{
    Serial.begin(115200);
    Serial.println(F("Mock ECU starting..."));

    pinMode(MCP_INT_PIN, INPUT);

    while (CAN.begin(MCP_ANY, CAN_SPEED, MCP_CRYSTAL) != CAN_OK)
    {
        Serial.println(F("MCP2515 init failed — check wiring/crystal. Retrying..."));
        delay(500);
    }

    CAN.setMode(MCP_NORMAL);
    Serial.println(F("MCP2515 ready. Sending CAN frames at 1 Mbps."));
}

/* ======================================================================
 * updateSimulation — advances the simulated sensor values over time
 * ==================================================================== */
static void updateSimulation(void)
{
    static uint32_t phase_ms = 0;
    static uint8_t  drive_phase = 0;   /* 0=idle 1=accel 2=cruise 3=decel */
    static uint32_t phase_start = 0;

    uint32_t now = millis();

    /* Change drive phase every ~5 seconds */
    if ((now - phase_start) > 5000UL)
    {
        phase_start = now;
        drive_phase = (drive_phase + 1) % 4;
    }

    /* Simulate RPM / speed / throttle per drive phase */
    switch (drive_phase)
    {
        case 0: /* Idle */
            sim.rpm_target = 800.0f + 50.0f * sinf(now / 3000.0f);
            sim.throttle   = 0.0f;
            break;
        case 1: /* Acceleration */
            sim.rpm_target = 3000.0f + 500.0f * sinf(now / 2000.0f);
            sim.speed     += 0.8f;
            if (sim.speed > 80.0f) sim.speed = 80.0f;
            sim.throttle   = 65.0f + 10.0f * sinf(now / 1000.0f);
            break;
        case 2: /* Cruise */
            sim.rpm_target = 2200.0f + 100.0f * sinf(now / 4000.0f);
            sim.throttle   = 30.0f + 5.0f * sinf(now / 2000.0f);
            break;
        case 3: /* Deceleration */
            sim.rpm_target = 1000.0f;
            sim.speed     -= 0.5f;
            if (sim.speed < 0.0f) sim.speed = 0.0f;
            sim.throttle   = 0.0f;
            break;
    }

    /* Smooth RPM towards target */
    float rpm_diff = sim.rpm_target - sim.rpm;
    sim.rpm += rpm_diff * 0.05f;

    /* Engine temp: warm up from ambient to ~90 °C, slight variation */
    if (sim.eng_temp < 90.0f)
        sim.eng_temp += 0.02f;
    sim.eng_temp += 0.5f * sinf(now / 10000.0f);

    /* Battery: slight ripple */
    sim.battery = 12400.0f + 200.0f * sinf(now / 7000.0f)
                           + (sim.rpm > 1000.0f ? 400.0f : -100.0f);

    /* Clamp values */
    sim.rpm        = constrain(sim.rpm,      600.0f,  8000.0f);
    sim.speed      = constrain(sim.speed,    0.0f,    200.0f);
    sim.eng_temp   = constrain(sim.eng_temp, -20.0f,  130.0f);
    sim.battery    = constrain(sim.battery,  10000.0f, 16000.0f);
    sim.throttle   = constrain(sim.throttle, 0.0f,    100.0f);

    (void)phase_ms;
}

/* ======================================================================
 * sendCANFrame — sends one CAN frame for the given PID
 * ==================================================================== */
static void sendCANFrame(uint8_t pid)
{
    uint8_t  buf[8] = {0};
    uint8_t  dlc    = 1;
    uint16_t id     = 0;

    switch (pid)
    {
        case 0: /* RPM — 2 bytes big-endian */
        {
            id   = CAN_ID_RPM;
            dlc  = 2;
            uint16_t rpm_u = (uint16_t)sim.rpm;
            buf[0] = (uint8_t)(rpm_u >> 8);
            buf[1] = (uint8_t)(rpm_u & 0xFF);
            break;
        }
        case 1: /* Speed */
        {
            id     = CAN_ID_SPEED;
            dlc    = 1;
            buf[0] = (uint8_t)constrain(sim.speed, 0, 255);
            break;
        }
        case 2: /* Engine temp (offset +40 so -40 °C = 0, 215 °C = 255) */
        {
            id     = CAN_ID_ENGINE_TEMP;
            dlc    = 1;
            buf[0] = (uint8_t)((int)sim.eng_temp + 40);
            break;
        }
        case 3: /* Battery mV — 2 bytes big-endian */
        {
            id   = CAN_ID_BATTERY;
            dlc  = 2;
            uint16_t bat_u = (uint16_t)sim.battery;
            buf[0] = (uint8_t)(bat_u >> 8);
            buf[1] = (uint8_t)(bat_u & 0xFF);
            break;
        }
        case 4: /* Throttle */
        {
            id     = CAN_ID_THROTTLE;
            dlc    = 1;
            buf[0] = (uint8_t)constrain(sim.throttle, 0, 100);
            break;
        }
        default:
            return;
    }

    byte rc = CAN.sendMsgBuf(id, 0 /* standard frame */, dlc, buf);
    if (rc != CAN_OK)
    {
        Serial.print(F("CAN TX error on ID 0x"));
        Serial.print(id, HEX);
        Serial.print(F(": "));
        Serial.println(rc);
    }
}

/* ======================================================================
 * loop
 * ==================================================================== */
void loop()
{
    uint32_t now = millis();

    /* Update simulation every loop iteration */
    updateSimulation();

    /* Send one PID per TX_INTERVAL_MS, cycling through all 5 PIDs */
    if ((now - sim.last_tx) >= TX_INTERVAL_MS)
    {
        sim.last_tx = now;
        sendCANFrame(sim.pid_index);

        /* Debug print every 5 frames (after a full PID cycle) */
        if (sim.pid_index == 4)
        {
            Serial.print(F("RPM:"));    Serial.print((uint16_t)sim.rpm);
            Serial.print(F(" SPD:"));   Serial.print((uint8_t)sim.speed);
            Serial.print(F(" TMP:"));   Serial.print((int8_t)sim.eng_temp);
            Serial.print(F(" BAT:"));   Serial.print((uint16_t)sim.battery);
            Serial.print(F(" TPS:"));   Serial.println((uint8_t)sim.throttle);
        }

        sim.pid_index = (sim.pid_index + 1) % 5;
    }
}
