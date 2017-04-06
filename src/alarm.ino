void setAlarmState(String value){
        if (value == "DISARM" || value == "disarmed") { alarmState = alarmStates[0]; }
        if (value == "ARM_HOME" || value == "armed_home") { alarmState = alarmStates[1]; }
        if (value == "ARM_AWAY" || value == "armed_away") { alarmState = alarmStates[2]; }
        if (value == "pending") { alarmState = alarmStates[3]; }
        if (value == "triggered") { alarmState = alarmStates[4]; }
        alarmNode.setProperty("state").send(alarmState);
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
        if (alarmState == alarmStates[0]) { lastDisarmedTime = millis(); }
}
void homeCheck(){
        if (alarmState == alarmStates[1]) {
                lastArmedHomeTime = millis();
                for (size_t i = 0; i < SENSOR_ARRAY_HOME_SIZE; i++) {
                        for (size_t j = 0; j < 10; j++) {
                                if (ReceivedSignal[j][0].toInt() == sensorArrayHome[i] &&
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
        if (alarmState == alarmStates[2]) {
                lastArmedAwayTime = millis();
                for (size_t i = 0; i < SENSOR_ARRAY_AWAY_SIZE; i++) {
                        for (size_t j = 0; j < 10; j++) {
                                if (ReceivedSignal[j][0].toInt()  == sensorArrayAway[i] &&
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
        if (alarmState == alarmStates[3]) {
                lastPendingTime =  millis();
                if (millis() > (PendingCounter + TIME_TO_TRIGGER * 1000UL)) {
                        if (PendingCounter > 0) {
                                alarmState = "triggered";
                                Serial.print(String(alarmState) + " from pending");
                        }
                        PendingCounter = millis();
                }
        }

}

void triggeredCheck(){
        if (alarmState == alarmStates[4]) {
                lastTriggeredTime = millis();
                if (millis() > (initialAlarmStateTime + (ALARM_INTERVAL * 1000UL)/4) || initialAlarmState == 0) {
                        initialAlarmStateTime = millis();
                        alarmNode.setProperty("state").send(alarmState);
                        // Serial << "ALARM ALARM ALARM" << endl;
                        initialAlarmState = 1;
                }
        }
}
