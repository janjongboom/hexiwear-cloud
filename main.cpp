#include "mbed.h"
#include "security.h"
#include "easy-connect.h"
#include "simple-mbed-client.h"
#include "Adafruit_SSD1351.h"
#include "FXOS8700Q.h"

Serial pc(USBTX, USBRX);
SimpleMbedClient client;

static DigitalOut led(LED1);
static DigitalOut blinkLed(LED2);
static InterruptIn btn(PTA13);

static DigitalOut boosten(PTC13, 1); // oled power enable
static Adafruit_SSD1351 oled(PTB20, PTE6, PTD15, PTB21, PTB22); // Hexiwear pins for SPI bus OLED (CS, RS, DC, SCK, MOSI, MISO);
static FXOS8700Q_acc acc(PTC11, PTC10, FXOS8700CQ_SLAVE_ADDR0); // Hexiwear I2C
static FXOS8700Q_mag mag(PTC11, PTC10, FXOS8700CQ_SLAVE_ADDR0); // Hexiwear I2C

static Ticker accelTicker;
static Ticker ledTicker;

Semaphore updates(0);

void patternUpdated(string v) {
    printf("New pattern: %s\n", v.c_str());
}

void lcdTextUpdated(string v) {
    oled.fillScreen(BLACK);
    oled.printf(v.c_str());
}

void lcdColorUpdated(int v) {
    uint8_t r = (v >> 16) & 0xff;
    uint8_t g = (v >> 8) & 0xff;
    uint8_t b = v & 0xff;

    oled.setTextColor(oled.Color565(r, g, b));
}

SimpleResourceInt btn_count = client.define_resource("button/0/clicks", 0, M2MBase::GET_ALLOWED);
SimpleResourceString pattern = client.define_resource("led/0/pattern", "500:500:500:500:500:500:500", &patternUpdated);

SimpleResourceInt lcd_color = client.define_resource("lcd/0/color", 0x00ff000, &lcdColorUpdated);
SimpleResourceString lcd_text = client.define_resource("lcd/0/text", "...", &lcdTextUpdated);

SimpleResourceInt acc_x = client.define_resource("accelerometer/0/x", 0, M2MBase::GET_ALLOWED);
SimpleResourceInt acc_y = client.define_resource("accelerometer/0/y", 0, M2MBase::GET_ALLOWED);
SimpleResourceInt acc_z = client.define_resource("accelerometer/0/z", 0, M2MBase::GET_ALLOWED);

void fall() {
    updates.release();
}

void toggleLed() {
    led = !led;
}

void readAccelerometer() {
    MotionSensorDataUnits acc_data;
    acc.getAxis(acc_data);

    acc_x = static_cast<int>(acc_data.x * 1000.0f);
    acc_y = static_cast<int>(acc_data.y * 1000.0f);
    acc_z = static_cast<int>(acc_data.z * 1000.0f);

    pc.printf("data is %d %d %d\r\n", static_cast<int>(acc_x), static_cast<int>(acc_y), static_cast<int>(acc_z));
}

void registered() {
    pc.printf("Registered\r\n");

    accelTicker.attach(&readAccelerometer, 2.0f);
}
void unregistered() {
    pc.printf("Unregistered\r\n");
}

void play(void* args) {
    stringstream ss(pattern);
    string item;
    while(getline(ss, item, ':')) {
        wait_ms(atoi((const char*)item.c_str()));
        blinkLed = !blinkLed;
    }
}

int main() {
    pc.baud(115200);

    btn.fall(&fall);

    ledTicker.attach(&toggleLed, 1.0f);

    // Write the current value of the lcd_text cloud variable to the screen
    oled.begin();
    oled.setRotation(0);
    lcdColorUpdated(lcd_color); // set initial text color
    oled.setTextWrap(false);
    oled.on();
    oled.fillScreen(BLACK);
    oled.printf(static_cast<string>(lcd_text).c_str());

    // enable the accelerometer
    acc.enable();

    client.define_function("led/0/play", &play);

    NetworkInterface* network = easy_connect(true);
    if (!network) {
        return 1;
    }

    struct MbedClientOptions opts = client.get_default_options();
    opts.ServerAddress = MBED_SERVER_ADDRESS;
    bool setup = client.setup(opts, network);
    if (!setup) {
        printf("Client setup failed\n");
        return 1;
    }

    client.on_registered(&registered);
    client.on_unregistered(&unregistered);

    while (1) {
        int v = updates.wait(25000);
        if (v == 1) {
            btn_count = btn_count + 1;
            printf("Button count is now %d\n", static_cast<int>(btn_count));
        }
        client.keep_alive();
    }
}
