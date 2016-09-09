#include <SPI.h>
    #include "Adafruit_MAX31855.h"
 
    #define DO   12
    #define CS   11
    #define CLK  10
    Adafruit_MAX31855 thermocouple(CLK, CS, DO);
 
    void setup() {
       Serial.begin(9600);
       Serial.println("MAX31855 test");
       // wait for MAX chip to stabilize
       delay(500);
    }
    void loop() {
       // Initialize variables.
       int i = 0; // Counter for arrays
       double internalTemp = thermocouple.readInternal(); // Read the internal temperature of the MAX31855.
       double rawTemp = thermocouple.readCelsius(); // Read the temperature of the thermocouple. This temp is compensated for cold junction temperature.
       double thermocoupleVoltage= 0;
       double internalVoltage = 0;
       double correctedTemp = 0;
 
        // Check to make sure thermocouple is working correctly.
       if (isnan(rawTemp)) {
        Serial.println("Something wrong with thermocouple!");
      }
       else {
          // Steps 1 & 2. Subtract cold junction temperature from the raw thermocouple temperature.
          thermocoupleVoltage = (rawTemp - internalTemp)*0.041276;  // C * mv/C = mV
 
          // Step 3. Calculate the cold junction equivalent thermocouple voltage.
 
          if (internalTemp >= 0) { // For positive temperatures use appropriate NIST coefficients
             // Coefficients and equations available from http://srdata.nist.gov/its90/download/type_k.tab
 
             double c[] = {-0.176004136860E-01,  0.389212049750E-01,  0.185587700320E-04, -0.994575928740E-07,  0.318409457190E-09, -0.560728448890E-12,  0.560750590590E-15, -0.320207200030E-18,  0.971511471520E-22, -0.121047212750E-25};
 
             // Count the the number of coefficients. There are 10 coefficients for positive temperatures (plus three exponential coefficients),
             // but there are 11 coefficients for negative temperatures.
             int cLength = sizeof(c) / sizeof(c[0]);
 
             // Exponential coefficients. Only used for positive temperatures.
             double a0 =  0.118597600000E+00;
             double a1 = -0.118343200000E-03;
             double a2 =  0.126968600000E+03;
 
 
             // From NIST: E = sum(i=0 to n) c_i t^i + a0 exp(a1 (t - a2)^2), where E is the thermocouple voltage in mV and t is the temperature in degrees C.
             // In this case, E is the cold junction equivalent thermocouple voltage.
             // Alternative form: C0 + C1*internalTemp + C2*internalTemp^2 + C3*internalTemp^3 + ... + C10*internaltemp^10 + A0*e^(A1*(internalTemp - A2)^2)
             // This loop sums up the c_i t^i components.
             for (i = 0; i < cLength; i++) {
                internalVoltage += c[i] * pow(internalTemp, i);
             }
                // This section adds the a0 exp(a1 (t - a2)^2) components.
                internalVoltage += a0 * exp(a1 * pow((internalTemp - a2), 2));
          }
          else if (internalTemp < 0) { // for negative temperatures
             double c[] = {0.000000000000E+00,  0.394501280250E-01,  0.236223735980E-04, -0.328589067840E-06, -0.499048287770E-08, -0.675090591730E-10, -0.574103274280E-12, -0.310888728940E-14, -0.104516093650E-16, -0.198892668780E-19, -0.163226974860E-22};
             // Count the number of coefficients.
             int cLength = sizeof(c) / sizeof(c[0]);
 
             // Below 0 degrees Celsius, the NIST formula is simpler and has no exponential components: E = sum(i=0 to n) c_i t^i
             for (i = 0; i < cLength; i++) {
                internalVoltage += c[i] * pow(internalTemp, i) ;
             }
          }
 
          // Step 4. Add the cold junction equivalent thermocouple voltage calculated in step 3 to the thermocouple voltage calculated in step 2.
          double totalVoltage = thermocoupleVoltage + internalVoltage;
 
          // Step 5. Use the result of step 4 and the NIST voltage-to-temperature (inverse) coefficients to calculate the cold junction compensated, linearized temperature value.
          // The equation is in the form correctedTemp = d_0 + d_1*E + d_2*E^2 + ... + d_n*E^n, where E is the totalVoltage in mV and correctedTemp is in degrees C.
          // NIST uses different coefficients for different temperature subranges: (-200 to 0C), (0 to 500C) and (500 to 1372C).
          if (totalVoltage < 0) { // Temperature is between -200 and 0C.
             double d[] = {0.0000000E+00, 2.5173462E+01, -1.1662878E+00, -1.0833638E+00, -8.9773540E-01, -3.7342377E-01, -8.6632643E-02, -1.0450598E-02, -5.1920577E-04, 0.0000000E+00};
 
             int dLength = sizeof(d) / sizeof(d[0]);
             for (i = 0; i < dLength; i++) {
                correctedTemp += d[i] * pow(totalVoltage, i);
             }
          }
          else if (totalVoltage < 20.644) { // Temperature is between 0C and 500C.
             double d[] = {0.000000E+00, 2.508355E+01, 7.860106E-02, -2.503131E-01, 8.315270E-02, -1.228034E-02, 9.804036E-04, -4.413030E-05, 1.057734E-06, -1.052755E-08};
             int dLength = sizeof(d) / sizeof(d[0]);
             for (i = 0; i < dLength; i++) {
                correctedTemp += d[i] * pow(totalVoltage, i);
             }
          }
          else if (totalVoltage < 54.886 ) { // Temperature is between 500C and 1372C.
             double d[] = {-1.318058E+02, 4.830222E+01, -1.646031E+00, 5.464731E-02, -9.650715E-04, 8.802193E-06, -3.110810E-08, 0.000000E+00, 0.000000E+00, 0.000000E+00};
             int dLength = sizeof(d) / sizeof(d[0]);
             for (i = 0; i < dLength; i++) {
                correctedTemp += d[i] * pow(totalVoltage, i);
             }
          } else { // NIST only has data for K-type thermocouples from -200C to +1372C. If the temperature is not in that range, set temp to impossible value.
             // Error handling should be improved.
             Serial.print("Temperature is out of range. This should never happen.");
             correctedTemp = NAN;
          }
 
          Serial.print("Corrected Temp = ");
          Serial.println(correctedTemp, 5);
          Serial.println("");
 
       }
 
       delay(1000);
 
    }
