#include <Homie.h>
#include <IRremoteESP8266.h>
#include <RCSwitch.h>
#include <DHT.h>

#define RF_EMITTER_PIN 15
#define RF_RECEIVER_PIN 5
#define RF_EMITTER_REPEAT 20
#define IR_EMITTER_PIN 14
#define IR_RECEIVER_PIN 2
#define DHT_PIN 4
#define DHT_TYPE DHT11
#define DHT_INTERVAL 300                 // in seconds
#define ALARM_INTERVAL 60              // in seconds
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
long arrayHome[10] = {0,0,0,0,0,0,0,0,0,0};
long arrayAway[10] = {0,0,0,0,0,0,0,0,0,0};
int arrayMQTT[6] = {0,0,0,0,0,0};

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
        getSensorArrayAway();
        getSensorArrayHome();
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
                //              Homie.getLogger() << "〽 DTH loop:" << endl;
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
                //              Homie.getLogger() << "〽 433Mhz loop:" << endl;
                Homie.getLogger() << " • Receiving 433Mhz value: " << mySwitch.getReceivedValue();
                Homie.getLogger() << " bitLenght: " << mySwitch.getReceivedBitlength();
                Homie.getLogger() << " protocol: " << mySwitch.getReceivedProtocol();
                Homie.getLogger() << " delay: " << mySwitch.getReceivedDelay() << endl;
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
                //              Homie.getLogger() << "〽 IR loop:" << endl;
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
        delay(50);
}
// MQTT -> 433 loop
bool rfSwitchOnHandler(const HomieRange& range, const String& value) {
        Homie.getLogger() << "〽 rfSwitchOnHandler(range," << value << ")" << endl;
        bool result = false;
        arrayMQTT[0] = 0;     //data or address
        arrayMQTT[1] = 350;   //pulseLength
        arrayMQTT[2] = 1;     //protocol
        arrayMQTT[3] = 0;     //group (1-> send to all group) (only for typeE)
        arrayMQTT[4] = 0;     //unit (15 first, 14 second, 13 third) (only for typeE)
        arrayMQTT[5] = 1;     //1:set ON,2:set OFF (only for typeE)

        getArrayMQTT(value);

        mySwitch.setPulseLength(arrayMQTT[1]);
        mySwitch.setProtocol(arrayMQTT[2]);

        if (arrayMQTT[0] > 0 && arrayMQTT[4] == 0) {
                Homie.getLogger() << " • Receiving MQTT > 433Mhz pulseLength: " << arrayMQTT[1] << " protocol: "<< arrayMQTT[2] <<" value: " << arrayMQTT[0] << endl;
                mySwitch.send(arrayMQTT[0], 24);
        }
        if (arrayMQTT[0] > 0 && arrayMQTT[3] == 0 && arrayMQTT[4] > 0) {
                Homie.getLogger() << " • Receiving MQTT > 433Mhz Address: " << arrayMQTT[0] << " unit: "<< arrayMQTT[4] <<" group: false" << endl;
                if(arrayMQTT[5] == 1) {
                        mySwitch.switchOn(arrayMQTT[0], false, arrayMQTT[4]);
                } else {
                        mySwitch.switchOff(arrayMQTT[0],false, arrayMQTT[4]);
                }
        }
        if (arrayMQTT[0] > 0 && arrayMQTT[3] == 1 && arrayMQTT[4] > 0) {
                Homie.getLogger() << " • Receiving MQTT > 433Mhz Address: " << arrayMQTT[0] << " unit: "<< arrayMQTT[4] <<" group: true" << endl;

                if(arrayMQTT[5] == 1) {
                        mySwitch.switchOn(arrayMQTT[0], false, arrayMQTT[4]);
                } else {
                        mySwitch.switchOff(arrayMQTT[0],false, arrayMQTT[4]);
                }
        }
        if(arrayMQTT[5] == 1) {
                boolean result = rfSwitchNode.setProperty("on").send(String(arrayMQTT[0]));
        } else {
                boolean result = rfSwitchNode.setProperty("off").send(String(arrayMQTT[0]));
        }
        if (result) Homie.getLogger() << " • 433Mhz pulseLength: " << arrayMQTT[1] << "  value: " << arrayMQTT[0] << " sent"<< endl;
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
        arrayMQTT[0] = 0; //data or address
        arrayMQTT[1] = 1; // IR protocol: 0:IR_COOLIX,1:IR_NEC,2:IR_Whynter,3:IR_LG,4:IR_Sony,5:IR_DISH,6:IR_RC5,7:IR_Sharp,8:IR_SAMSUNG
        arrayMQTT[2] = 0; //not used
        arrayMQTT[3] = 0; //not used
        arrayMQTT[4] = 0; //not used
        arrayMQTT[5] = 0; //not used

        getArrayMQTT(value);

        Homie.getLogger() << " • Receiving MQTT > IR Protocol: " << arrayMQTT[1] << " value: " << arrayMQTT[0] << endl;
        boolean signalSent = false;
        if (arrayMQTT[1] == 0) {irsend.sendCOOLIX(arrayMQTT[0], 24);
                                signalSent = true;}
        if (arrayMQTT[1] == 1) {irsend.sendNEC(arrayMQTT[0], 32);
                                signalSent = true;}
        if (arrayMQTT[1] == 2) {irsend.sendWhynter(arrayMQTT[0], 32);
                                signalSent = true;}
        if (arrayMQTT[1] == 3) {irsend.sendLG(arrayMQTT[0], 28);
                                signalSent = true;}
        if (arrayMQTT[1] == 4) {irsend.sendSony(arrayMQTT[0], 12);
                                signalSent = true;}
        if (arrayMQTT[1] == 5) {irsend.sendDISH(arrayMQTT[0], 16);
                                signalSent = true;}
        if (arrayMQTT[1] == 6) {irsend.sendRC5(arrayMQTT[0], 12);
                                signalSent = true;}
        if (arrayMQTT[1] == 7) {irsend.sendSharpRaw(arrayMQTT[0], 15);
                                signalSent = true;}
        if (arrayMQTT[1] == 8) {irsend.sendSAMSUNG(arrayMQTT[0], 32);
                                signalSent = true;}
        if (signalSent) {
                boolean result = rfSwitchNode.setProperty("on").send(String(arrayMQTT[0]));
                if (result) Homie.getLogger() << " • IR protocol: " << arrayMQTT[1] << "  value: " << arrayMQTT[0] << " sent"<< endl;
        }
        irrecv.enableIRIn();
        return true;
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
        rfSwitchNode.advertise("off").settable(rfSwitchOnHandler);
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
