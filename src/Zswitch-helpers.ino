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
                        Homie.getLogger() << "〽 getChannelByCode("<<  currentCode << ")" << endl <<  " • mapping: " << mapping << endl;
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
        Homie.getLogger() << "〽 storeValue("<<  currentCode << ")" << endl <<  " • minimum index: " << o << endl;
        // replace it by the new one
        ReceivedSignal[o][0] = currentCode;
        ReceivedSignal[o][1] = now;
        ReceivedSignal[o][2] = alarmState;
        Homie.getLogger() << " • Col: value / timestamp / alarmstate" << endl;
        for (int i = 0; i < 10; i++)
        {
                Homie.getLogger() << " • " << String(i) << ": " << ReceivedSignal[i][0] << " / " << String(ReceivedSignal[i][1]) << " / " << String(ReceivedSignal[i][2]) << endl;
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
        Homie.getLogger() << "〽 isAduplicate("<<  value << ")" << endl;
        for (int i=0; i<10; i++) {
                if (ReceivedSignal[i][0] == value) {
                        long now = millis();
                        if (now - ReceivedSignal[i][1].toInt() < TIME_AVOID_DUPLICATE * 1000UL) { // change
                                Homie.getLogger() << " • Duplicate found, don't send" << endl;
                                return true;
                        }
                }
        }
        Homie.getLogger() << " • not dulicated, will be sent" << endl;
        return false;
}
