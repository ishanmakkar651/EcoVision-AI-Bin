#include <ESP32Servo.h>

// ==== Pin Definitions ====
#define TRIG_PIN     12
#define ECHO_PIN     13
#define INFRARED_PIN 14
#define MOISTURE_PIN 4
#define SERVO_PIN_1  16  // Organic
#define SERVO_PIN_2  15  // Recyclable
#define SERVO_PIN_3  2   // Non-Recyclable

// ==== Servo Setup ====
Servo organicServo, recyclableServo, nonRecyclableServo;

// ==== Constants ====
const long MAX_DISTANCE = 50;    // Reduced for 3.3V operation
const int MOISTURE_DRY = 30;
const int MOISTURE_WET = 60;
const int NUM_SAMPLES = 7;       // Better noise immunity
const int SERVO_DELAY = 800;     // ms

// ==== Setup ====
void setup() {
  Serial.begin(115200);
  delay(1000);

  // Sensor configuration
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);       // CRITICAL: No pullup/pulldown
  pinMode(INFRARED_PIN, INPUT);

  // Servo setup
  ESP32PWM::allocateTimer(0);
  organicServo.attach(SERVO_PIN_1);
  recyclableServo.attach(SERVO_PIN_2);
  nonRecyclableServo.attach(SERVO_PIN_3);
  closeAllBins();

  Serial.println("SYSTEM READY - 3.3V ULTRASONIC MODE");
}

// ==== Ultrasonic Functions ====
long getDistanceCM() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 40000); // 40ms timeout
  if(duration <= 0) return -1;  // Error code
  return duration * 0.0343 / 2;
}

long getSafeDistance() {
  long validReadings = 0;
  long total = 0;

  for(int i=0; i<NUM_SAMPLES; i++){
    long dist = getDistanceCM();
    if(dist > 0 && dist < 200){  // Filter outliers
      total += dist;
      validReadings++;
    }
    delay(25);
  }
  
  if(validReadings == 0){
    Serial.println("[ULTRASONIC] Sensor error!");
    return -1;
  }
  return total / validReadings;
}

// ==== IR & Moisture ====
bool isObjectPresent() {
  int detections = 0;
  for(int i=0; i<5; i++){
    detections += !digitalRead(INFRARED_PIN); // Active LOW
    delay(3);
  }
  return (detections >= 3);
}

int getMoisture() {
  int raw = analogRead(MOISTURE_PIN);
  return map(raw, 1500, 3000, 0, 100); // Calibrate these values!
}

// ==== Servo Control ====
void closeAllBins() {
  organicServo.write(0);
  recyclableServo.write(0);
  nonRecyclableServo.write(0);
}

void openBin(int bin) {
  const char* names[] = {"ORGANIC", "RECYCLABLE", "NON-RECYCLABLE"};
  Servo* servos[] = {&organicServo, &recyclableServo, &nonRecyclableServo};
  
  if(bin <1 || bin>3) return;
  
  servos[bin-1]->write(90);
  Serial.print("[ACTION] ");
  Serial.print(names[bin-1]);
  Serial.println(" bin opened");
  delay(SERVO_DELAY);
  closeAllBins();
}

// ==== Main Logic ====
void loop() {
  long distance = getSafeDistance();
  
  Serial.print("[DEBUG] Distance: ");
  if(distance == -1) Serial.println("ERROR");
  else Serial.print(distance), Serial.println("cm");

  if(distance == -1 || distance > MAX_DISTANCE){
    Serial.println("Status: No object detected");
    delay(1000);
    return;
  }

  if(!isObjectPresent()){
    Serial.println("IR Rejected: No confirmation");
    delay(1000);
    return;
  }

  int moisture = getMoisture();
  Serial.print("Moisture: "), Serial.print(moisture), Serial.println("%");

  int bin = 3; // Default
  if(moisture >= MOISTURE_WET) bin = 1;
  else if(moisture <= MOISTURE_DRY) bin = 2;

  openBin(bin);
  delay(2000); // Cooldown
}