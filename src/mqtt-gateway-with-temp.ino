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
#define DHT_INTERVAL 60                 // in seconds
#define ALARM_INTERVAL 60               // in seconds
#define TIME_AVOID_DUPLICATE 3          // in seconds
#define TIME_TO_TRIGGER 30              // in seconds

// Alarm
String ReceivedSignal[10][3] ={{"0", "0", "N/A"},{"0", "0", "N/A"},{"0", "0", "N/A"},{"0", "0", "N/A"},{"0", "0", "N/A"},
                               {"0", "0", "N/A"},{"0", "0", "N/A"},{"0", "0", "N/A"},{"0", "0", "N/A"},{"0", "0", "N/A"}};
const String alarmStates[] = {"disarmed","armed_home","armed_away","pending","triggered"};
String alarmState = alarmStates[0];
long lastArmedHomeTime = 0;
long lastPendingTime = 0;
long lastArmedAwayTime = 0;
long lastDisarmedTime = 0;
long lastTriggeredTime = 0;
long PendingCounter = 0;
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
DHT dht(DHT_PIN, DHT_TYPE);
unsigned long DHTlastSent = 0;

// homie nodes & settings
HomieNode alarmNode("Alarm", "switch");
HomieNode rfSwitchNode("MQTTto433", "switch");
HomieNode rfReceiverNode("433toMQTT", "switch");
HomieNode irSwitchNode("MQTTtoIR", "switch");
HomieNode irReceiverNode("IRtoMQTT", "switch");
HomieNode temperatureNode("DHT", "temperature");

// HomieSetting<long> temperatureIntervalSetting("temperatureInterval", "The temperature interval in seconds");
// HomieSetting<double> temperatureOffsetSetting("temperatureOffset", "The temperature offset in degrees");
HomieSetting<const char*> channelMappingSetting("channels", "Mapping of 433MHz & IR signals to mqtt channel.");


void setupHandler() {
        temperatureNode.setProperty("unit").send("C");
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
                float humidity = dht.readHumidity();
                float temperature = dht.readTemperature();
                float heatIndex = dht.computeHeatIndex(temperature, humidity, false);
                temperatureNode.setProperty("temperature").send(String(temperature));
                temperatureNode.setProperty("heatIndex").send(String(heatIndex));
                temperatureNode.setProperty("humidity").send(String(temperature));
                DHTlastSent = millis();
        }
// 433 -> MQTT loop
        if (mySwitch.available()) {
                long data = mySwitch.getReceivedValue();
                mySwitch.resetAvailable();
                Serial << "Receiving 433Mhz > MQTT signal: " << data << endl;
                String currentCode = String(data);
                if (!isAduplicate(currentCode) && currentCode!=0) {
                        String channelId = getChannelByCode(currentCode);
                        Serial << "Code: " << currentCode << " matched to channel " << channelId << endl;
                        boolean result = rfReceiverNode.setProperty("channel-" + channelId).send(currentCode);
                        if (result) storeValue(currentCode);
                }
        }
// IR -> MQTT loop
        if (irrecv.decode(&results)) {
                long data = results.value;
                irrecv.resume();
                Serial << "Receiving IR > MQTT signal: " << data << endl;
                String currentCode = String(data);
                if (!isAduplicate(currentCode) && currentCode!=0) {
                        String channelId = getChannelByCode(currentCode);
                        Serial << "Code: " << currentCode << " matched to channel " << channelId << endl;
                        boolean result = irReceiverNode.setProperty("channel-" + channelId).send(currentCode);
                        if (result) storeValue(currentCode);
                }

        }
}
// getChannel helper
String getChannelByCode(const String & currentCode) {
        String mappingConfig = channelMappingSetting.get();
        String mapping = "";
        String codes = "";
        int lastIndex = 0;
        int lastCodeIndex = 0;
        for (int i = 0; i < mappingConfig.length(); i++) {
                if (mappingConfig.substring(i, i + 1) == ";") {
                        mapping = mappingConfig.substring(lastIndex, i);
                        Serial << "mapping: " << mapping << endl;
                        codes = mapping.substring(mapping.indexOf(':') + 2, mapping.length() - 1);
                        for (int j = 0; j < codes.length(); j++) {
                                if (codes.substring(j, j + 1) == ",") {
                                        if (currentCode.indexOf(codes.substring(lastCodeIndex, j)) > -1) {
                                                return mapping.substring(0, mapping.indexOf(':'));;
                                        }
                                        codes = codes.substring(j + 1, codes.length());
                                }
                        }
                        if (currentCode.indexOf(codes) > -1) {
                                return mapping.substring(0, mapping.indexOf(':'));;
                        }
                        lastIndex = i + 1;
                }
        }
        return "0";
}
// ReceivedSignal store
void storeValue(String currentCode){
        long now = millis();
        // find oldest value of the buffer
        int o = getMin();
        Serial << "Minimum index: " << String(o) << endl;
        // replace it by the new one
        ReceivedSignal[o][0] = currentCode;
        ReceivedSignal[o][1] = now;
        ReceivedSignal[o][2] = alarmState;
        Serial << "send this code : " << String(ReceivedSignal[o][0]) << "/" << String(ReceivedSignal[o][1]) << "/" << String(ReceivedSignal[o][2]) << endl << "Col: value/timestamp/alarmstate";
        for (int i = 0; i < 10; i++)
        {
                Serial << String(i) << ":" << ReceivedSignal[i][0] << "/" << String(ReceivedSignal[i][1]) << "/" << String(ReceivedSignal[i][2]) << endl;
        }
}
// ReceivedSignal helpers
int getMin(){
        int minimum = 0;
        minimum = ReceivedSignal[0][1].toInt();
        int minindex=0;
        for (int i = 0; i < 10; i++)
        {
                if (ReceivedSignal[i][1].toInt() < minimum) {
                        minimum = ReceivedSignal[i][1].toInt();
                        minindex = i;
                }
        }
        return minindex;
}
//433 & IR duplicate check
boolean isAduplicate(String value){
        Serial << "Check if value is a duplicate" << endl;
        for (int i=0; i<10; i++) {
                if (ReceivedSignal[i][0] = value) {
                        long now = millis();
                        if (now - ReceivedSignal[i][1].toInt() < TIME_AVOID_DUPLICATE * 1000UL) { // change
                                Serial << "don't send the received code" << endl;
                                return true;
                        }
                }
        }
        return false;
}
// MQTT -> 433 loop
bool rfSwitchOnHandler(const HomieRange & range, const String & value) {
        long int data = 0;
        int pulseLength = 350;
        if (value.indexOf(',') > 0) {
                pulseLength = atoi(value.substring(0, value.indexOf(',')).c_str());
                data = atoi(value.substring(value.indexOf(',') + 1).c_str());
        } else {
                data = atoi(value.c_str());
        }
        Serial << "Receiving MQTT > 433Mhz signal: " << pulseLength << ":" << data << endl;
        mySwitch.setPulseLength(pulseLength);
        mySwitch.send(data, 24);
        rfSwitchNode.setProperty("on").send(String(data));
        return true;
}
// MQTT -> Alarm status loop
bool alarmSwitchOnHandler(const HomieRange & range, const String & value) {
        String data = value.c_str();
        Serial << "Receiving MQTT alarm signal: " << data << endl;
        setAlarmState(data);
        return true;
}
// MQTT -> IR loop
bool irSwitchOnHandler(const HomieRange & range, const String & value) {
        long int data = 0;
        String irProtocol = "IR_NEC";
        if (value.indexOf(',') > 0) {
                irProtocol = atoi(value.substring(0, value.indexOf(',')).c_str());
                data = atoi(value.substring(value.indexOf(',') + 1).c_str());
        } else {
                data = atoi(value.c_str());
        }
        Serial << "Receiving MQTT > IR signal: " << data << endl;
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
                if (result) Serial << "IR protocol: " << irProtocol << " IR signal: " << data << " sent"<< endl;
                irrecv.enableIRIn();
                return true;
        }
}
void setup() {
        Serial.begin(115200);
        Serial << endl << endl;
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
        Homie.disableResetTrigger();
        alarmNode.advertise("state");
        alarmNode.advertise("set").settable(alarmSwitchOnHandler);
        rfSwitchNode.advertise("on").settable(rfSwitchOnHandler);
        rfReceiverNode.advertise("channel-0");
        irSwitchNode.advertise("on").settable(irSwitchOnHandler);
        irReceiverNode.advertise("channel-0");
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
