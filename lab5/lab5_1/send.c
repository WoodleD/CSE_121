#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <gpiod.h>

#define CHIP_NAME "gpiochip0"
#define LED_PIN   18
#define UNIT_US   80000					// 1 Morse time unit = 80 ms

static const char *morse[36] =				// A-Z, 0-9 in Morse Code  
{	
	".-", "-...", "-.-.", "-..", ".", "..-.", "--.", "....", "..", ".---",
	"-.-", ".-..", "--", "-.", "---", ".--.", "--.-", ".-.", "...", "-",
	"..-", "...-", ".--", "-..-", "-.--", "--..", 
	"-----", ".----", "..---", "...--", "....-", 
	".....", "-....", "--...", "---..", "----."
};

static struct gpiod_line *led; 

static const char *lookup(char c)
{
	// must convert letter to uppercase, then subtract 'A' to index it in morse[]
	if (isalpha((unsigned char)c)) return morse[toupper(c) - 'A']; 

	// index the digit by subtracting '0' char, then add 26, 
	// since numbers on morse[] start at 26
	if (isdigit((unsigned char)c)) return morse[26 + (c - '0')];
	
	return NULL;					// if no valid character was sent, return NULL 
}

static void sendMessage(const char *msg)
{
	for (int i = 0; msg[i]; i++)			// iterate through msg string 
	{
		if (msg[i] == ' ')			// when a space is encountered
		{
			printf("/ ");			// a forward slash is printed
			fflush(stdout); 
			usleep(4 * UNIT_US); 		// wait another 4 time units
			continue;			// before continuing the loop
		}

		const char *code = lookup(msg[i]); 	// get string corresponding to msg[i] char
		if (!code) continue; 			// if code was NULL, continue loop and skip char  
		
		printf("%s ", code); 			// print the corresponding code
		fflush(stdout); 

		// flash LED loop 
		for (int j = 0; code[j]; j++)		// iterate through the current char code 
		{
			gpiod_line_set_value(led, 1);   // turn led on for this character
			usleep((code[j] == '.' ? 1 : 3) * UNIT_US); // if character is a dot, on for 1 time unit. otherwise, on for 3. 
			gpiod_line_set_value(led, 0); 	// turn led off 
			usleep(UNIT_US); 		// 1 time unit gap between symbols 
		}
		usleep(2 * UNIT_US); 			// 1 + 2 = 3 time units between letters 
	}
	printf("\n"); 					// endline after a message has printed 
}

int main(int argc, char *argv[])
{
	if (argc != 3)					// program expects a number followed by a string
	{
		fprintf(stderr, "Usage: ./send <count> \"message\" \n"); 
		return 1; 				// return error 
	}	
	int count = atoi(argv[1]); 			// convert our first char into an int 
	
	struct gpiod_chip *chip = gpiod_chip_open_by_name(CHIP_NAME);		// open up gpio chip 
	if (!chip) { perror("open chip"); return 1; } 				// if error, return 1 
	
	led = gpiod_chip_get_line(chip, LED_PIN); 				// set led struct to gpio line 
	if (!led || gpiod_line_request_output(led, "morse", 0) < 0)		// if led is null or gpio line fails 
	{
		perror("request line"); 					// print an error 
		gpiod_chip_close(chip); 					// close chip 
		return 1; 
	}

	for (int i = 0; i < count; i++)						// loop for however many counts the user specified 
	{
		sendMessage(argv[2]);						// call sendMessage function 
		usleep(7 * UNIT_US); 						// pause for 7 time units between repeats
	}

	gpiod_line_set_value(led, 0);						// make sure to turn off led
	gpiod_line_release(led);						// release gpio line
	gpiod_chip_close(chip);							// close chip
	return 0; 
}
