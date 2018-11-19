#include <Arduino.h>

const int analogPin = 1;
const int handshakePin = 13;
int cFLag = true;

void setup() {
    init();
    Serial.begin(9600);
    Serial3.begin(9600);
    Serial.flush();
    Serial3.flush();
}

/*
    The following function will create and return the randomkey generated by reading analog pin 1.
*/
uint32_t randomKeyGenerator() {
    uint32_t parameter = 1;
    uint32_t randomKey = 0;
    // The following for loop will extract the LSB from the value read by analogRead until 
    // 16 bits are extracted and then will convert them to an unsigned integer.
    for (int i = 0; i < 32; i++) {
        uint16_t num = analogRead(analogPin);
        randomKey += parameter * (num & 1);
        parameter *= 2;
        delay(50);
    }
    return randomKey;
}

uint32_t mulMod(uint32_t a, uint32_t b, uint32_t m) {
    uint32_t result = 0;
    uint32_t newA = a % m;
    uint32_t newB = b % m;
    for (int i = 0; i < 32; i++) {
        if ((newA >> i) & 1) { // evalutates to true iff i'th bit of a is 1
            result += newB % m;
            result = result % m;
        }
        newB = (newB << 1) % m;
    }
    return result;
}

uint32_t powMod(uint32_t g, uint32_t a, uint32_t p) {
    // compute gp[0] = g
    // gp[1] = gp[0] * ap[0]
    // ...
    // gp[i] = gp[i-1] * gp[i-1] (all mod p) for i >= 1

    uint32_t result = 1 % p;
    uint32_t sqrVal = g % p; //stores g ^ {2 ^ i} values, initially 2 ^ {2 ^ 0}
    uint32_t newA = a;

    // LOOP INVARIANT: statements that hold throughout a loop
    //                 with the goal of being convinced the loop works
    // statements: just before iteration i,
    //               result = g ^ {binary number represented the first i bits of a}
    //               sqrVal = g ^ {2 ^ i}
    //               newB = a >> i
    while (newA > 0) {
        if (newA & 1) { // evalutates to true iff i'th bit of b is 1
            result = mulMod(result, sqrVal, p);
        }
        sqrVal = mulMod(sqrVal, sqrVal, p);
        newA = (newA >> 1);
    }
    return result;
}

uint32_t publicKeyAGenerator() {
    uint32_t randomKey = 0;
    randomKey = randomKeyGenerator();
    Serial.println("Random key:");
    Serial.println(randomKey);
    Serial.flush();
    uint32_t publicKeyA = 0;
    publicKeyA = powMod(16807, randomKey, 2147483647);
    Serial.println("Public key A:");
    Serial.println(publicKeyA);
    Serial.flush();
    return publicKeyA;
}

/*
    Waits for a certain number of bytes on Serial3 or timeout 
    @ param nbytes: the number of bytes we want
    @ param timeout: timeout period (ms); specifying a negative number
                turns off timeouts (the function waits indefinitely
                if timeouts are turned off).
    @ return True if the required number of bytes have arrived.
*/
bool wait_on_serial3(uint8_t nbytes, long timeout) {
  unsigned long deadline = millis() + timeout; // wraparound not a problem
  while (Serial3.available() < nbytes && (timeout < 0 || millis() < deadline)) 
  {
    delay(1); // be nice, no busy loop
  }
  return Serial3.available() >= nbytes;
}

/*
    Writes an uint32_t to Serial3, starting from the least-significant
    and finishing with the most significant byte. 
*/
void uint32_to_serial3(uint32_t num) {
  Serial3.write((char) (num >> 0));
  Serial3.write((char) (num >> 8));
  Serial3.write((char) (num >> 16));
  Serial3.write((char) (num >> 24));
}

/*
    Reads an uint32_t from Serial3, starting from the least-significant
    and finishing with the most significant byte. 
*/
uint32_t uint32_from_serial3() {
  uint32_t num = 0;
  num = num | ((uint32_t) Serial3.read()) << 0;
  num = num | ((uint32_t) Serial3.read()) << 8;
  num = num | ((uint32_t) Serial3.read()) << 16;
  num = num | ((uint32_t) Serial3.read()) << 24;
  return num;
}

uint32_t secretKeyGenerator(uint32_t publicKeyA, uint32_t publicKeyB, uint32_t p){
    uint32_t secretKey = 0;
    secretKey = powMod(publicKeyB, publicKeyA, p);
    return secretKey;
}

uint32_t client(uint32_t publicKeyA, uint32_t p) {
    uint32_t secretKey = 0;
    uint32_t publicKeyB = 0;
    
    enum states {Start, WaitingForAck, DataExchange};
    states currentState = Start;
    while (true) {
        switch (currentState) {
            case Start: {
                Serial3.write("C");
                uint32_to_serial3(publicKeyA);
                Serial3.flush();
                currentState = WaitingForAck;
            }
            case WaitingForAck: {
                if (wait_on_serial3(5, 1000) == true && Serial3.read() == 'A') {
                    publicKeyB = uint32_from_serial3();
                    Serial3.write("A");
                    Serial3.flush();
                    currentState = DataExchange;
                }
                else {
                    currentState = Start;
                }
            }
            case DataExchange: {
                secretKey = secretKeyGenerator(publicKeyA, publicKeyB, p);
                return secretKey;
            }
        }
    }
}

uint32_t server(uint32_t publicKeyA, uint32_t p) {
    uint32_t secretKey = 0;
    uint32_t publicKeyB = 0;
    enum states {Listen, WaitingForKey0, WaitForAck0, WaitingForKey1, WaitForAck1, DataExchange};
    states currentState = Listen;
    while (true) {
        switch (currentState) {
            case Listen: {
                while (Serial3.read() == -1 || Serial3.read() != 'C') {
                    delay(1);
                }
                currentState = WaitingForKey0;
            }
            case WaitingForKey0: {
                if (wait_on_serial3(4, 1000) == true) {
                    publicKeyB = uint32_from_serial3();
                    Serial3.write("A");
                    uint32_to_serial3(publicKeyA);
                    Serial3.flush();
                    currentState = WaitForAck0;
                }
                else {
                    currentState = Listen;
                }
            }
            case WaitForAck0: {
                if (wait_on_serial3(1, 1000) == true && Serial3.read() == 'C') {
                    currentState = WaitingForKey1;
                }
                else if (wait_on_serial3(1, 1000) == true && Serial3.read() == 'A') {
                    currentState = DataExchange;
                }
                else {
                    currentState = Listen;
                }
            }
            case WaitingForKey1: {
                if (wait_on_serial3(4, 1000) == true) {
                    publicKeyB = uint32_from_serial3();
                    currentState = WaitForAck1;
                }
                else {
                    currentState = Listen;
                }
            }
            case WaitForAck1: {
                if (wait_on_serial3(1, 1000) == true && Serial3.read() == 'C') {
                    currentState = WaitingForKey1;
                }
                else if (wait_on_serial3(1, 1000) == true && Serial3.read() == 'A') {
                    currentState = DataExchange;
                }
                else {
                    currentState = Listen;
                }
            }
            case DataExchange: {
                secretKey = secretKeyGenerator(publicKeyA, publicKeyB, p);
                return secretKey;
            }
        }
    }
}

uint32_t handShake(uint32_t publicKeyA, uint32_t p) {
    uint32_t secretKey = 0;
    if (digitalRead(handshakePin) == LOW) {
        secretKey = client(publicKeyA, p);
    }
    else {
        secretKey = server(publicKeyA, p);
    }
    return secretKey;
}

uint32_t next_key(uint32_t current_key) {
	if (cFlag){
		return current_key;
		cFlag = false
	else{
		const uint32_t modulus = 0x7FFFFFFF; // 2^31-1
		const uint32_t consta = 48271;  // we use that consta<2^16
		uint32_t lo = consta * (current_key & 0xFFFF);
		uint32_t hi = consta * (current_key >> 16);
		lo += (hi & 0x7FFF) << 16;
		lo += hi >> 15;
		if (lo > modulus) lo -= modulus;
		return lo;
}
/*
	The following function will read in the encrypted message from other Arduino and print it on the display
*/
void recieveMessage(uint32_t secretKey) {
	// The following if statement has been implemented from the discussion in class.
	if (Serial3.available() > 0) {
		uint32_t incomingByte = 0;
		// Read the encrypted message from the connected Arduino.
		incomingByte = Serial3.read();
		uint32_t decryptedByte = 0;
		//getting a diffrent key to imporove the security
		secretKey = next_key(secretKey);
		// Call decryption function to receive the decrypted message from encrypted.
		decryptedByte = decryption(incomingByte, secretKey);
		// Write the decrypted message to the user display.
		Serial.write(decryptedByte);
		Serial.flush();
		Serial3.flush();
	}
}
/*
	The following function will read in the message from the serial monitor and send the encrypted.
*/
void sendMessage(uint32_t secretKey) {
	// The following if statement has been implemented from the discussion in class.
	if (Serial.available() > 0) {
		uint32_t incomingByte = 0;
		// Read the user input.
		incomingByte = Serial.read();
		// Display the user input.
		Serial.write(incomingByte);
		// Flush instead of delay to wait instead of immediately moving on to next block of code.
		Serial.flush();
		Serial3.flush();
		//getting a diffrent key to imporove the security
		secretKey = next_key(secretKey);
		// Call encryption function to generate the encrypted message to the user input.
		uint32_t encryptedByte = 0;
		encryptedByte = encryption(incomingByte, secretKey);
		// The following will send the encrypted message to the connect Arduino.
		Serial3.write(encryptedByte);
		Serial.flush();
		Serial3.flush();
		// When the user presses enter, a newline character is printed and return character is sent to the other
		// Arduino.
		if (incomingByte == 13) {
			Serial.write("\n");
			Serial.flush();
			Serial3.flush();
			encryptedByte = encryption(10, secretKey);
			Serial3.write(encryptedByte);
		}
	}
}

/*
	The following function will take decrypt the encrypted message received using XOR encryption.
*/
uint32_t decryption(uint32_t incomingByte, uint32_t secretKey) {
	// klow will be used to apply XOR with encrypted message (incomingByte).
	uint32_t klow = powMod(secretKey, 1, 256);
	uint32_t decryptedByte = 0;
	decryptedByte = incomingByte ^ klow;
	return decryptedByte;
}

/*
	The following function will take encrypt the message to be sent using XOR encryption.
*/
uint32_t encryption(uint32_t incomingByte, uint32_t secretKey) {
	// klow will be used to apply XOR with the message (incomingByte).
	uint32_t klow = powMod(secretKey, 1, 256);
	uint32_t encryptedByte = 0;
	encryptedByte = incomingByte ^ klow;
	return encryptedByte;
}
int main() {
    setup();
    uint32_t publicKeyA = 0;
    publicKeyA = publicKeyAGenerator();
    uint32_t secretKey = 0;
    secretKey = handShake(publicKeyA, 2147483647);
    Serial.flush();
    Serial3.flush();
    while (true) {
		sendMessage(secretKey);
		recieveMessage(secretKey);
	}
    return 0;
}
