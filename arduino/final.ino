#include <LiquidCrystal.h>  // LCD Display
#include <Servo.h>

// Initialize LCD
LiquidCrystal lcd(8, 9, 10, 11, 12, 13);

// Init Servo variables
Servo myservo;    // create servo object
int pos = 0;      // variable to store the servo position

// GPIO
// int inPin = 2;    // Input pin - Now A1
int val = 0;
int outPin = 4;   // Output pin
int scaledValue;  // Init potentiometer
int co2val = -1;
int new_co2val = -1;
int tempval = -1;


void setup()
{
  /************** Prepare LCD Vars ***************/
  // LCD Welcome screen
  lcd.begin(16, 2);
  lcd.setCursor(0, 0);
  lcd.print("  EC535  Final");
  lcd.setCursor(0, 1);
  lcd.print("Burnham & Sharif");

  // Attach Servo  
  myservo.attach(3);

  // Beagle Signals
  // pinMode(inPin, INPUT);
  pinMode(outPin, OUTPUT);

  // Start serial communication
  Serial.begin(9600); 
  delay(2000);
}


void loop() 
{
  // Read CO2
  Serial.println("Z\r\n");
  delay(100);
  String response1 = "";
  while (Serial.available()) {
    char c1 = Serial.read();
    response1 += c1;
  }
  response1.trim();
  if (response1.length() > 3) {
    response1.remove(response1.length() - 3);
    new_co2val = response1.substring(2).toInt();
    if (new_co2val > 100) {
      co2val = new_co2val;
    }
  }

  // Read Temp
  Serial.println("T\r\n");
  delay(100);
  String response2 = "";
  while (Serial.available()) {
    char c2 = Serial.read();
    response2 += c2;
  }
  response2.trim();
  if (response2.length() > 3) {
    response2.remove(response2.length() - 3);
    tempval = (response2.substring(2).toInt() - 1000)/10;
  }
  
  // Read A0 and print all vals to LCD
  scaledValue = map(analogRead(A0), 0, 1023, 0, 20);
  scaledValue = 100*scaledValue;
  update_lcd(co2val, tempval, scaledValue);

  /************** Update GPIO ***************/
  // Servo move on IN_2 HIGH
  // val = digitalRead(inPin);
  // Read A0
  val = analogRead(A1);
  if (val > 200) {
    myservo.write(90);
  } else {
    myservo.write(0);
  }

  // Set HIGH when CO2 exceeds threshold
  if (co2val > scaledValue) {
    digitalWrite(outPin, HIGH);
  } else {
    digitalWrite(outPin, LOW);
  }
  delay(200); 
}


// Function to update the LCD
void update_lcd (int co2val, int tempval, int threshold)
{
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("CO2: ");

  lcd.setCursor(4, 0);
  lcd.print(co2val);
  lcd.print(" PPM");

  lcd.setCursor(13, 0);
  lcd.print(tempval);
  lcd.print("C");

  lcd.setCursor(0, 1);
  lcd.print("Thresh: ");

  lcd.setCursor(7, 1);
  lcd.print(threshold);
  lcd.print(" PPM");
}
