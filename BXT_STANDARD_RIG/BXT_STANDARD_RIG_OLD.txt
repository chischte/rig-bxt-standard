#include <SPI.h>
#include <Controllino.h>

#include <SPI.h>
#include <Controllino.h>

#include <SPI.h>
#include <Controllino.h>

#include <SPI.h>
#include <Controllino.h>

#include <SPI.h>
#include <Controllino.h>

#include <Controllino.h>

unsigned long stepState = 1;
unsigned long previousMillis;
unsigned long currentMillis;
long OnTime;
long DelTime;
int endSwitch = 0;	//Endschalter Bremszyl.
int strapDet;		//Schalter Banderkennung
int buttonState;	//Ein-Ausschalter
int lastButtonState;//Merker Starttaste
int runState;		//Betriebsstatus
int coolTime = 0; //Abk�hlzeit
bool autoMode = false; //Betriebsmodus 0=manuell, 1= Automatik

class Cylinder
{
	int cylPin;				// Nummer des Cylinderpins		
	int CylState;           // Setzt den Zylinderstatus							

public:						// Konstruktor f�r Cylinder:
	Cylinder(int pin)		// Initialisieren von Variablen und Status
	{
		cylPin = pin;
		pinMode(cylPin, OUTPUT);

		CylState = HIGH;
		previousMillis = 0;
	}
	void Shutoff()
	{
		CylState = LOW;
		digitalWrite(cylPin, CylState);
	}

	void Update(long on, long del, bool endState) // Dauer Ein, Pause, Zustand nach Ablauf der Zeit
	{
		OnTime = on;
		DelTime = del;
		currentMillis = millis();
		if (currentMillis - previousMillis < OnTime) // Ist die Einschaltdauer schon abgelaufen?
		{
			CylState = HIGH;
			digitalWrite(cylPin, CylState);
		}
		else
		{
			if (endState == true)
			{
				CylState = LOW;  // Zylinder zur�ck fahren				
				digitalWrite(cylPin, CylState);  // den Zylinder updaten
			}
		}
		if (currentMillis - previousMillis > (OnTime + DelTime)) // Ist die Pausenzeit schon abgelaufen?
		{
			if (autoMode == true) {
				stepState++;
				previousMillis = currentMillis; // Zeit merken
			}
			else
			{
				if (buttonState == HIGH)
				{
					stepState++;
					previousMillis = currentMillis; // Zeit merken
				}
			}
			
		}
	}
};

int startbutton()
{
	buttonState = digitalRead(CONTROLLINO_A3); //Ein-Aus-Taster abfragen mit Selbsthaltung
	if (buttonState != lastButtonState)
	{
		if (buttonState == HIGH)
		{
			if (runState == 1)
			{
				runState = 0;
			}
			else
			{
				runState = 1;
			}
		}
		lastButtonState = buttonState;
	}
	return runState;
};

int modeswitch()
{
	int switchState = digitalRead(CONTROLLINO_A2); //Betriebsmodusschalter abfragen
	if (switchState == HIGH)
	{
		autoMode = true;
	}
	else
	{
		autoMode = false;
	}
	return autoMode;
};

Cylinder cyl1(6); //Band klemmen
Cylinder cyl2(7); //Band spannen
Cylinder cyl3(5); //Bremszylinder
Cylinder cyl4(8); //Band Schweissen
Cylinder cyl5(9); //Wippenhebel ziehen
Cylinder cyl6(10); //Band schneiden

void setup()
{
	pinMode(CONTROLLINO_A0, INPUT);	//Endschalter Bremszylinder
	pinMode(CONTROLLINO_A1, INPUT);	//Banddetektierung
	pinMode(CONTROLLINO_A2, INPUT);	//Start Stop oder Schrittbetrieb
	pinMode(CONTROLLINO_A3, INPUT);	//Automatik Ein/Aus	
	pinMode(CONTROLLINO_A4, INPUT);			//variable Abk�hlzeit
}

void loop()
{
	modeswitch();
	strapDet = digitalRead(CONTROLLINO_A1); //Banddetektierung abfragen
	buttonState = digitalRead(CONTROLLINO_A3); //Ein-Aus-Taster abfragen
	if ((autoMode == false) & (strapDet == 0) & (buttonState == HIGH) & (stepState <= 1)) {
		stepState = 2;
	}
		
	if(autoMode == true)
	{ 	startbutton();
	if ((runState == 0) || (strapDet == 1))
	{
		stepState = 1;	//alle Zylinder ausschalten
	}
	else
	{
		if (stepState < 2)
		{
			stepState++;
			previousMillis = currentMillis; // Zeit merken
		}
	}
	}
	

int potVal = analogRead(CONTROLLINO_A4); //einlesen Poti 0-1023?
	coolTime = (5000 + (potVal * 12));

	switch (stepState)
	{
	case 1: //alle Zylinder ausschalten
		cyl3.Shutoff();
		cyl2.Shutoff();
		cyl4.Shutoff();
		cyl5.Shutoff();
		cyl6.Shutoff();
		cyl1.Shutoff();
		break;
	case 2: //Bremszylinder zur�ck fahren
		cyl3.Update(2500, 500, true);
		break;
	case 3: //Wippenhebel ziehen
		endSwitch = digitalRead(CONTROLLINO_A0); //Endschalter abfragen  
    if (endSwitch == 0) //Wenn Endschalter nicht aktiv dann Wippenhebel ziehen
		{
		  cyl5.Update(1500, 1000, true);
		}
		break;
	case 4: //Band vorschieben
		cyl2.Update(450, 250, true);
   		break;
	case 5: //Band klemmen
		cyl1.Update(500, 500, false);
		break;
	case 6: //Band spannen
		cyl2.Update(3000, 50, true);
		break;
	case 7: // ist Endschalter aktiviert?
		/*endSwitch = digitalRead(CONTROLLINO_A0); //Endschalter abfragen	
		if (endSwitch == 1) //Wenn Endschalter aktiv, dann gehe zu 8 
   
		{ */
			stepState = 8;
		// }
   		break;
	case 8: //Band schneiden
    cyl6.Update(2000, 2000, true);
    break;
	
	case 9: //Schweissen
		cyl4.Update(500, coolTime, true);
		break;
	
	case 10: //Wippenhebel ziehen
		cyl5.Update(1500, 1000, true);
		break;
	
	case 11: //Bandklemme l�sen
		cyl1.Update(300, 1000, true);
		break;
	

	}
	if (stepState == 12)
	{
		stepState = 1;
	}
}
