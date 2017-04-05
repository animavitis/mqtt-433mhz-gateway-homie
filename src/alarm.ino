void setAlarmState(String value){
                if (value == "DISARM" || value == "disarmed") {
                        alarmState = alarmStates[0];
                }
                if (value == "ARM_HOME" || value == "armed_home") {
                        alarmState = alarmStates[1];
                }
                if (value == "ARM_AWAY" || value == "armed_away") {
                        alarmState = alarmStates[2];
                }
                if (value == "pending") {
                        alarmState = alarmStates[3];
                }
                if (value == "triggered") {
                        alarmState = alarmStates[4];
                }
                //        boolean result = client.publish(STATETOPIC, alarmState.c_str());
                Serial << "Alarm state set to: " << alarmState << endl;
}
void setAlarmTimes(){
        lastArmedHomeTime = millis();
        lastPendingTime = millis();
        lastArmedAwayTime = millis();
        lastDisarmedTime = millis();
        lastTriggeredTime = millis();
}
void disarmCheck(){
        if (alarmState == "disarmed") {
                lastDisarmedTime = millis();
                //  Serial << "alarm disarmed" << endl;
        }
}
void homeCheck(){
        if (alarmState == "armed_home") {
                lastArmedHomeTime = millis();
                for (size_t i = 0; i < SENSORARRAYHOME_SIZE; i++) {
                        for (size_t j = 0; j < 10; j++) {
                                if (ReceivedSignal[j][0].toInt() == SENSORARRAYHOME[i] &&
                                    ReceivedSignal[j][1].toInt()  < lastArmedHomeTime &&
                                    ReceivedSignal[j][1].toInt()  > lastDisarmedTime &&
                                    ReceivedSignal[j][1].toInt()  > lastArmedAwayTime &&
                                    ReceivedSignal[j][1].toInt()  > lastPendingTime
                                    ) {
                                        alarmState = "pending";
                                        PendingCounter = millis();
                                        Serial << String(alarmState) << " from armed_home" << endl;
                                }
                        }
                }
        }
}
void awayCheck(){
        if (alarmState == "armed_away") {
                lastArmedAwayTime = millis();
                for (size_t i = 0; i < SENSORARRAYAWAY_SIZE; i++) {
                        for (size_t j = 0; j < 10; j++) {
                                if (ReceivedSignal[j][0].toInt()  == SENSORARRAYAWAY[i] &&
                                    ReceivedSignal[j][1].toInt()  < lastArmedAwayTime &&
                                    ReceivedSignal[j][1].toInt()  > lastDisarmedTime &&
                                    ReceivedSignal[j][1].toInt()  > lastArmedHomeTime &&
                                    ReceivedSignal[j][1].toInt()  > lastPendingTime
                                    ) {
                                        alarmState = "pending";
                                        PendingCounter = millis();
                                          Serial << String(alarmState) << " from armed_away" << endl;
                                }
                        }
                }
        }
}
void pendingCheck(){
        if (alarmState == "pending") {
                lastPendingTime =  millis();
                if (millis() > (PendingCounter + TimeToTrigger)) {
                        if (PendingCounter > 0) {
                                alarmState = "triggered";
                                Serial.print(String(alarmState) + " from pending");
                        }
                        PendingCounter = millis();
                }
        }

}

void triggeredCheck(){
        if (alarmState == "triggered") {
                lastTriggeredTime = millis();
                if (millis() > (initialAlarmStateTime + 60000) || initialAlarmState == 0) {
                        initialAlarmStateTime = millis();
                        alarmNode.setProperty("state").send(alarmState);
                        // Serial << "ALARM ALARM ALARM" << endl;
                        initialAlarmState = 1;
                }
        }
}
