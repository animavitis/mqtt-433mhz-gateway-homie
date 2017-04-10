
void setAlarmState(String value){
        if (value == "DISARM" || value == "disarmed") {
                alarmStateOld = alarmState;
                alarmState = alarmStates[0];
                alarmStateTarget = alarmStates[0];
        }
        if (value == "ARM_HOME" || value == "armed_home") {
                alarmStateOld = alarmState;
                alarmState = alarmStates[3];
                alarmStateTarget = alarmStates[1];
                pendingCounter = millis();
                pendingStatusSent = false;
        }
        if (value == "ARM_AWAY" || value == "armed_away") {
                alarmStateOld = alarmState;
                alarmState = alarmStates[3];
                alarmStateTarget = alarmStates[2];
                pendingCounter = millis();
                pendingStatusSent = false;
        }
        if (value == "pending") {
                alarmStateOld = alarmState;
                alarmState = alarmStates[3];
                alarmStateTarget = alarmStateOld;
                pendingCounter = millis();
        }
        if (value == "triggered") {
                alarmStateOld = alarmState;
                alarmState = alarmStates[3];
                alarmStateTarget = alarmStates[4];
                pendingCounter = millis();
                pendingStatusSent = false;
        }
        alarmNode.setProperty("state").send(alarmState);
        Homie.getLogger() << "〽 setAlarmState("<<  value << ")" << endl <<  " • alarmState was: " << alarmStateOld << " is: " << alarmState << " will be: "  << alarmStateTarget << endl;
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
                                        alarmStateOld = alarmState;
                                        alarmState = alarmStates[3];
                                        alarmStateTarget = alarmStates[4];
                                        pendingCounter = millis();
                                        pendingStatusSent = false;
                                        Homie.getLogger() << "〽 homeCheck()" << endl <<  " • alarmState was: " << alarmStateOld << " is: " << alarmState << " will be: "  << alarmStateTarget << endl;
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
                                if (ReceivedSignal[j][0].toInt() == sensorArrayHome[i] &&
                                    ReceivedSignal[j][1].toInt()  < lastArmedHomeTime &&
                                    ReceivedSignal[j][1].toInt()  > lastDisarmedTime &&
                                    ReceivedSignal[j][1].toInt()  > lastArmedAwayTime &&
                                    ReceivedSignal[j][1].toInt()  > lastPendingTime
                                    ) {
                                        alarmStateOld = alarmState;
                                        alarmState = alarmStates[3];
                                        alarmStateTarget = alarmStates[4];
                                        pendingCounter = millis();
                                        pendingStatusSent = false;
                                        Homie.getLogger() << "〽 awayCheck()" << endl <<  " • alarmState was: " << alarmStateOld << " is: " << alarmState << " will be: "  << alarmStateTarget << endl;
                                }
                        }
                }
        }
}
void pendingCheck(){
        if (alarmState == alarmStates[3]) {
                lastPendingTime =  millis();
                if (millis() > (pendingCounter + TIME_TO_TRIGGER * 1000UL)) {
                        if (pendingCounter > 0) {
                                alarmState = alarmStateTarget;
                                alarmNode.setProperty("state").send(alarmState);
                                Homie.getLogger() << "〽 pendingCheck()" << endl <<  " • alarmState was: " << alarmStateOld << " is: " << alarmState << endl;
                        }
                        pendingCounter = millis();
                } else {
                        if(!pendingStatusSent) {
                                alarmNode.setProperty("state").send(alarmState);
                                pendingStatusSent = true;
                        }
                }
        }
}
void triggeredCheck(){
        if (alarmState == alarmStates[4]) {
                lastTriggeredTime = millis();
                if (millis() > (initialAlarmStateTime + (ALARM_INTERVAL * 1000UL)/4) || initialAlarmState == 0) {
                        initialAlarmStateTime = millis();
                        alarmNode.setProperty("state").send(alarmState);
                        Homie.getLogger() << "〽 triggeredCheck()" << endl <<  " • alarmState was: " << alarmStateOld << " is: " << alarmState << endl;
                        initialAlarmState = 1;
                }
        }
}


void getSensorArmAway() {
        String alarmArrayConfig = sensorArrayAwaySetting.get(); // away:[123,234,345,456,567];home:[987,876];rest:[564]
        String armAway = "";
        unsigned long arrayAway[] = {0};
        String armHome = "";

        String armRest = "";
        int from = 0;
        int fromSub = 0;
        int arraySize = 0;
        for (int i = 0; i < alarmArrayConfig.length(); i++) {
                if (alarmArrayConfig.substring(i, i + 5) == ";home") {
                        armAway = alarmArrayConfig.substring(from + 5, i + 1);
                        from = i + 5;
                        for (int j = 0; j < armAway.length(); j++) {
                                fromSub = 0;
                                arraySize = 0;
                                if (armAway.substring(j, j + 1) == ",") {
                                        arrayAway[arraySize] = armAway.substring(fromSub,j).toInt();
                                        fromSub = j;
                                        arraySize++;
                                }
                        }
                }
                if (alarmArrayConfig.substring(i, i + 5) == ";rest" ) {
                        armHome = alarmArrayConfig.substring(from, i + 1);
                        from = i + 5;
                }

                // armRest = alarmArrayConfig.substring(from, alarmArrayConfig.length());
                // from = 0;
        }

}
