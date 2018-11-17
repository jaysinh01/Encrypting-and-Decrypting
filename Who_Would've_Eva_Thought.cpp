// --------------------------------------------------
// Name (ID): Ang Li (1550746) & Jaysinh Parmar (1532143)
//
// CMPUT 274, Fall 2018
//
// Assignment 2 Part 1: Simple Proof of Concept
// ---------------------------------------------------

#include <Arduino.h>

// Set up the pin used to read in random numbers.
const int analogPin = 1;

/*
    Initialize both serials.
*/
void setup() {
	init();
    Serial.begin(9600);
    Serial3.begin(9600);
    Serial.flush();
    Serial3.flush();
}

/*
    Given an array of characters str[] of length len.
    Reads characters from the serial monitor until len-1 are
    read or '\r' is encountered and stores them sequentially in str[].
    Adds the null terminator to str[] at the end of this sequence.

	read_int.cpp original class version, modified by Ang Li and Jaysinh Parmar, November 6th, 2018.
*/
void readString(char str[], int len) {
    // We didn't use a 'for' loop because we need the value of 'index' when
    // 'while' exits, so that we know where to add the null terminator '\0'.
    int index = 0;
    while (index < len - 1) {
        // If something is waiting to be read on Serial0.
        if (Serial.available() > 0) {
            char byteRead = Serial.read();
            // If the user press 'enter'.
            if (byteRead == '\r') {
                break;
            }
            else {
                str[index] = byteRead;
                index += 1;
            }
        }
    }
    str[index] = '\0';
}

/*
    Read a string (of digits 0-9) from the serial monitor by reading
    characters until 'enter' is pressed, and return an unsigned 32-bit 'int'.

    read_int.cpp original class version, modified by Ang Li and Jaysinh Parmar, November 6th, 2018.
*/
uint32_t readUnsigned32() {
    char str[32];
    readString(str, 32);
    return atol(str);
}

/*
    The following function will create and return the randomkey generated by reading analog pin 1.
*/
uint32_t keyGenerator() {
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

/*
    The following function will generate the secret key by reading the public key from serial monitor and
    generate the secret key.
    (i.e. Publickey ^ secretkey (created by keyGenerator fen)) mod 19211 (p))
    It will also print Public key which will be used by other Arduino to generate the secret key.
*/
uint32_t secretKeyGenerator(uint32_t PublicKey, uint32_t g, uint32_t p) {
	// Generate the PublicKeyA for the other Arduino.
	uint32_t PublicKeyA = 0;
    PublicKeyA = powMod(g, PublicKey, p);
    Serial.println("Your public key A is:");
    Serial.flush();
    Serial.println(PublicKeyA);
    Serial.flush();
    Serial.println("Please enter public B:");
    Serial.flush();
	// Read the PublicKey generated by other Arduino from the serial monitor.
    uint32_t PublicKeyB = 0;
    PublicKeyB = readUnsigned32();
	// Generate the secretkey using the privatekey generated from the keyGenerator and
    // the Publickey obtained from the serial monitor.
    uint32_t secretKey = 0;
    secretKey = powMod(PublicKeyB, PublicKey, p);
    Serial.println("Now the chat begin:");
    Serial.flush();
    return secretKey;
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
    The following function will read in the encrypted message from other Arduino and print it on the display
*/
void recieveMessage(uint32_t secretKey) {
	// The following if statement has been implemented from the discussion in class.
    if (Serial3.available() > 0) {
        uint32_t incomingByte = 0;
		// Read the encrypted message from the connected Arduino.
        incomingByte = Serial3.read();
        uint32_t decryptedByte = 0;
		// Call decryption function to receive the decrypted message from encrypted.
        decryptedByte = decryption(incomingByte, secretKey);
		// Write the decrypted message to the user display.
        Serial.write(decryptedByte);
        Serial.flush();
		Serial3.flush();
    }
}

int main() {
    setup();
    uint32_t randomKey = 0;
	// Generate the randomkey.
    randomKey = keyGenerator();
    uint32_t secretKey = 0;
	// Generate the secret key.
    secretKey = secretKeyGenerator(randomKey, 16807, 2147483647);
    Serial.flush();
    Serial3.flush();
	// The following while loop will continuously iterate and will execute
	// sendMessage and recieveMessage function.
    while (true) {
        sendMessage(secretKey);
        recieveMessage(secretKey);
    }
    return 0;
}