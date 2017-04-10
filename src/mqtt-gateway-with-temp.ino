#include <Homie.h>
#include <IRremoteESP8266.h>
#include <RCSwitch.h>
#include <DHT.h>

#define RF_EMITTER_PIN 12
#define RF_RECEIVER_PIN 5
#define RF_EMITTER_REPEAT 20
#define IR_EMITTER_PIN 14
#define IR_RECEIVER_PIN 2
#define DHT_PIN 4
#define DHT_TYPE DHT11
#define DHT_INTERVAL 300                 // in seconds
#define ALARM_INTERVAL 60               // in seconds
#define TIME_AVOID_DUPLICATE 3          // in seconds
#define TIME_TO_TRIGGER 30              // in seconds

// Alarm
String ReceivedSignal[10][3] ={{"N/A", "N/A", "N/A"},{"N/A", "N/A", "N/A"},{"N/A", "N/A", "N/A"},{"N/A", "N/A", "N/A"},{"N/A", "N/A", "N/A"},
                               {"N/A", "N/A", "N/A"},{"N/A", "N/A", "N/A"},{"N/A", "N/A", "N/A"},{"N/A", "N/A", "N/A"},{"N/A", "N/A", "N/A"}};

const String alarmStates[] = {"disarmed","armed_home","armed_away","pending","triggered"};
String alarmStateOld = alarmStates[0];
String alarmState = alarmStates[0];
String alarmStateTarget = alarmStates[0];
long lastArmedHomeTime = 0;
long lastPendingTime = 0;
long lastArmedAwayTime = 0;
long lastDisarmedTime = 0;
long lastTriggeredTime = 0;
long pendingCounter = 0;
bool pendingStatusSent = true;
long initialAlarmState = 0;
long initialAlarmStateTime = 0;

#define SENSOR_ARRAY_AWAY_SIZE 2
#define SENSOR_ARRAY_HOME_SIZE 1
unsigned long sensorArrayAway [SENSOR_ARRAY_AWAY_SIZE] = {13980949,2025705};
unsigned long sensorArrayHome [SENSOR_ARRAY_HOME_SIZE] = {2025705};

// RF Switch
RCSwitch mySwitch = RCSwitch();

// IR switch
IRrecv irrecv(IR_RECEIVER_PIN);
IRsend irsend(IR_EMITTER_PIN);
decode_results results;

// DHT temp sensor
DHT dht(DHT_PIN, DHT_TYPE, 11);
unsigned long DHTlastSent = 0;

// homie nodes & settings
HomieNode alarmNode("alarm", "switch");
HomieNode irSwitchNode("toIR", "switch");
HomieNode rfSwitchNode("toRF", "switch");
HomieNode receiverNode("toMQTT", "switch");
HomieNode temperatureNode("DHT", "temperature");

// HomieSetting<long> temperatureIntervalSetting("temperatureInterval", "The temperature interval in seconds");
// HomieSetting<double> temperatureOffsetSetting("temperatureOffset", "The temperature offset in degrees");
HomieSetting<const char*> channelMappingSetting("channels", "Mapping of 433MHz & IR signals to mqtt channel.");
HomieSetting<const char*> sensorArrayAwaySetting("arrayAway", "list of sensor for arm away");
HomieSetting<const char*> sensorArrayHomeSetting("arrayHome", "list of sensor for arm away");

void setupHandler() {
        temperatureNode.setProperty("unit").send("C");
        getSensorArmAway();
}
void loopHandler() {
//Alarm loop
        if (millis() > (initialAlarmStateTime + ALARM_INTERVAL * 1000UL) || initialAlarmState == 0) {
                initialAlarmStateTime = millis();
                alarmNode.setProperty("state").send(alarmState);
                initialAlarmState = 1;
        }
        disarmCheck();
        homeCheck();
        awayCheck();
        pendingCheck();
        triggeredCheck();
// DHT temp loop
        if (millis() - DHTlastSent >= DHT_INTERVAL * 1000UL || DHTlastSent == 0) {
                Homie.getLogger() << "〽 DTH loop:" << endl;
                float humidity = dht.readHumidity();
                float temperature = dht.readTemperature();
                float heatIndex = dht.computeHeatIndex(temperature, humidity, false);
                DHTlastSent = millis();
                if (temperature > 0 && humidity > 0 && heatIndex > 0) {
                        temperatureNode.setProperty("temperature").send(String(temperature));
                        temperatureNode.setProperty("heatIndex").send(String(heatIndex));
                        temperatureNode.setProperty("humidity").send(String(temperature));
                        Homie.getLogger() << " • temp: " << temperature << " humidity: " << humidity << " heatindex: " << heatIndex << endl;
                } else {
                        Homie.getLogger() << " • DTH sensor failed" << endl;
                }
        }
// 433 -> MQTT loop
        if (mySwitch.available()) {
                long data = mySwitch.getReceivedValue();
                Homie.getLogger() << "〽 433Mhz loop:" << endl;
                Homie.getLogger() << " • Receiving 433Mhz value: " << mySwitch.getReceivedValue() << " bitLenght: " << mySwitch.getReceivedBitlength() << " protocol: " << mySwitch.getReceivedProtocol() << endl;
                mySwitch.resetAvailable();
                String currentCode = String(data);
                if (!isAduplicate(currentCode) && currentCode!=0) {
                        String channelId = getChannelByCode(currentCode);
                        Homie.getLogger() << " • Code: " << currentCode << " matched to channel " << channelId << endl;
                        boolean result = receiverNode.setProperty("rf-" + channelId).send(currentCode);
                        if (result) storeValue(currentCode);
                }
        }
// IR -> MQTT loop
        if (irrecv.decode(&results)) {
                Homie.getLogger() << "〽 IR loop:" << endl;
                long data = results.value;
                irrecv.resume();
                Homie.getLogger() << " • Receiving IR signal: " << data << endl;
                String currentCode = String(data);
                if (!isAduplicate(currentCode) && currentCode!=0) {
                        String channelId = getChannelByCode(currentCode);
                        Homie.getLogger() << " • Code: " << currentCode << " matched to channel " << channelId << endl;
                        boolean result = receiverNode.setProperty("ir-" + channelId).send(currentCode);
                        if (result) storeValue(currentCode);
                }

        }


}





// MQTT -> 433 loop
bool rfSwitchOnHandler(const HomieRange& range, const String& value) {
  Homie.getLogger() << "〽 rfSwitchOnHandler(range," << value << ")" << endl;
        long int data = 0;
        int pulseLength = 350;
        if (value.indexOf(',') > 0) {
                pulseLength = atoi(value.substring(0, value.indexOf(',')).c_str());
                data = atoi(value.substring(value.indexOf(',') + 1).c_str());
        } else {
                data = atoi(value.c_str());
        }
        Homie.getLogger() << " • Receiving MQTT > 433Mhz pulseLength: " << pulseLength << " value: " << data << endl;
        mySwitch.setPulseLength(pulseLength);
        mySwitch.send(data, 24);
boolean result = rfSwitchNode.setProperty("on").send(String(data));
  if (result) Homie.getLogger() << " • 433Mhz pulseLength: " << pulseLength << "  value: " << data << " sent"<< endl;
        return true;
}
// MQTT -> Alarm status loop
bool alarmSwitchOnHandler(const HomieRange& range, const String& value) {
    Homie.getLogger() << "〽 alarmSwitchOnHandler(range," << value << ")" << endl;
        String data = value.c_str();
        Homie.getLogger() << " • Receiving MQTT alarm status: " << data << endl;
        setAlarmState(data);
        return true;
}
// MQTT -> IR loop
bool irSwitchOnHandler(const HomieRange& range, const String& value) {
      Homie.getLogger() << "〽 irSwitchOnHandler(range," << value << ")" << endl;
        long int data = 0;
        String irProtocol = "IR_NEC";
        if (value.indexOf(',') > 0) {
                irProtocol = atoi(value.substring(0, value.indexOf(',')).c_str());
                data = atoi(value.substring(value.indexOf(',') + 1).c_str());
        } else {
                data = atoi(value.c_str());
        }
        Homie.getLogger() << " • Receiving MQTT > IR Protocol: " << irProtocol << " value: " << data << endl;
        boolean signalSent = false;
        if (irProtocol == "IR_COOLIX") {irsend.sendCOOLIX(data, 24);
                                        signalSent = true;}
        if (irProtocol == "IR_NEC") {irsend.sendNEC(data, 32);
                                     signalSent = true;}
        if (irProtocol == "IR_Whynter") {irsend.sendWhynter(data, 32);
                                         signalSent = true;}
        if (irProtocol == "IR_LG") {irsend.sendLG(data, 28);
                                    signalSent = true;}
        if (irProtocol == "IR_Sony") {irsend.sendSony(data, 12);
                                      signalSent = true;}
        if (irProtocol == "IR_DISH") {irsend.sendDISH(data, 16);
                                      signalSent = true;}
        if (irProtocol == "IR_RC5") {irsend.sendRC5(data, 12);
                                     signalSent = true;}
        if (irProtocol == "IR_Sharp") {irsend.sendSharpRaw(data, 15);
                                       signalSent = true;}
        if (irProtocol == "IR_SAMSUNG") {irsend.sendSAMSUNG(data, 32);
                                         signalSent = true;}
        if (signalSent) {
                boolean result = irSwitchNode.setProperty("on").send(String(data));
                if (result) Homie.getLogger() << " • IR protocol: " << irProtocol << " value: " << data << " sent"<< endl;
                irrecv.enableIRIn();
                return true;
        }
        irrecv.enableIRIn();
}
void setup() {
        Serial.begin(115200);
        Homie.getLogger() << endl << endl;
        // init Alarm
        setAlarmTimes();

        // init IR library
        irsend.begin();
        irrecv.enableIRIn();

        // init RF library
        mySwitch.enableTransmit(RF_EMITTER_PIN); // RF Transmitter
        mySwitch.setRepeatTransmit(RF_EMITTER_REPEAT); //increase transmit repeat to avoid lost of rf sendings
        mySwitch.enableReceive(RF_RECEIVER_PIN); // Receiver on pin D3

        // init Homie
        Homie_setFirmware("mqtt-433-ir-alarm", "1.0.0");
        Homie.setSetupFunction(setupHandler).setLoopFunction(loopHandler);
        //    Homie.disableResetTrigger();
        alarmNode.advertise("state").settable(alarmSwitchOnHandler);
        receiverNode.advertise("rf-0");
        receiverNode.advertise("ir-0");
        irSwitchNode.advertise("on").settable(irSwitchOnHandler);
        rfSwitchNode.advertise("on").settable(rfSwitchOnHandler);
        temperatureNode.advertise("unit");
        temperatureNode.advertise("temperature");
        temperatureNode.advertise("heatIndex");
        temperatureNode.advertise("humidity");

        // temperatureOffsetSetting.setDefaultValue(DEFAULT_TEMPERATURE_OFFSET);
        Homie.setup();
}
void loop() {
        Homie.loop();
}
