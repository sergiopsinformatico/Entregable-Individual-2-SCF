/****************************************
*ENTREGABLE 2 SCF - SERGIO PEREZ SANCHEZ*
****************************************/

/*************************
*Importacion de Librerias*
*************************/

//Librerias Generales C y Arduino - WemosESP32
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <Arduino.h>

//Tareas FreeRTOS => Bibliografia Campus Virtual
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

//Pantalla OLED => Bibiliografia OLED
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

//Temperatura
#include <Tone32.h>
#include "DHT.h"

/*********************
*CONSTANTES DEFINIDAS*
*********************/

/*Botones para encender y apagar*/
#define switch1 26 //=> Switch SW1 (D2)
#define switch2 25 //=> Switch SW2 (D3)

//Para apagar al cargar el programa => No se usa en la aplicacion
#define R 13 //=> Led R del Led RGB
#define G 5 //=> Led G del Led RGB
#define B 23 //=> Led B del Led RGB

//Sensor Temperatura
#define temperatureDHT11 17 //=> Temperatura (D4 - DHT11)
DHT dht(temperatureDHT11, DHT11);

//Led de Notificacion
#define led2 19 //=> Led 2 (D12) donde se notifica cuando se conecte la caldera

//Potenciometro para umbral Temperatura
#define potentiometer 2 //=> Potenciometro (A0) donde se indica el umbral de la temperatura donde se activa la notificación de la caldera

/*Sensor de Luz*/
#define sensorLuz 4 //=> Sensor de la luz (A1) para apagado o encendido del sistema

/*Pantalla OLED LCD => Bibiliografia OLED*/
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

/******************
* TAREAS FREERTOS *
******************/

static TaskHandle_t tarea_encender_termostato;
static TaskHandle_t tarea_apagar_termostato;
static TaskHandle_t tarea_nivel_luz_ambiente;
static TaskHandle_t tarea_temperatura_ambiente;

/*************************
* ESTADOS DEL TERMOSTATO *
*************************/

#define TERMOSTATO_APAGADO 0 //=> Termostato Apagado
#define TERMOSTATO_ENCENDIDO 1 //=> Termostato Encendido
#define TERMOSTATO_STAND_BY 2 //=> Termostato Stand By (luz del ambiente)
int estado_termostato = TERMOSTATO_APAGADO;

/********************
* CODIGO APLICACION *
********************/

/*Nivel luz en el ambiente*/
static void nivel_luz_ambiente_manejador(void * pvParameters) {
    for( ;; ) {
        
        //Cada 5s
        int tiempoActual = millis() + 5000;
        do{}while(millis() <= tiempoActual);

        float medida_luz = analogRead(sensorLuz);

        if(medida_luz < 700){
            estado_termostato = TERMOSTATO_STAND_BY;

            String printApagadoMsg = "\nApagado Automatico (Stand By)";

            display.clearDisplay();
            display.setTextSize(1);
            display.setTextColor(WHITE);
            display.setCursor(1, 0);
            display.println(printApagadoMsg);
            display.display();
            Serial.println(printApagadoMsg);

        } else {
            estado_termostato = TERMOSTATO_ENCENDIDO;
        }

        if(estado_termostato == TERMOSTATO_ENCENDIDO){

            String printMsg = "\nNivel Luz: "+String(medida_luz);

            display.clearDisplay();
            display.setTextSize(1);
            display.setTextColor(WHITE);
            display.setCursor(1, 0);
            display.println(printMsg);
            display.display();
            Serial.println(printMsg);

        }
    }
}

/*Temperatura Ambiente y Enciende/Apaga Caldera*/
static void temperatura_ambiente_manejador(void * pvParameters) {
    for( ;; ) {
        
        //4 veces por segundo => Cada 250ms
        int tiempoActual = millis() + 250;
        do{}while(millis() <= tiempoActual);

        float temperatura = dht.readTemperature();
        float umbral = analogRead(potentiometer)/100;

        int umbralShow = umbral;

        String printMsg = "\nTemperatura: "+String(temperatura)+"ºC\nTemperatura Umbral: "+String(umbralShow)+"ºC";

        if (temperatura < umbral) {
            digitalWrite(led2,1);
            printMsg = printMsg+"\nCaldera Encendida";
        } else {
            digitalWrite(led2,0);
            printMsg = printMsg+"\nCaldera Apagada";
        }

        if(estado_termostato == TERMOSTATO_ENCENDIDO){
            display.clearDisplay();
            display.setTextSize(1);
            display.setTextColor(WHITE);
            display.setCursor(1, 0);
            display.println(printMsg);
            display.display();
            Serial.println(printMsg);
        }
        
        if(estado_termostato != TERMOSTATO_ENCENDIDO){
            digitalWrite(led2,0);
        }
    }
}

/*Funcion Encender Termostato*/
static void encender_termostato_manejador(void * pvParameters) {
    if(estado_termostato == TERMOSTATO_APAGADO){

        if(tarea_nivel_luz_ambiente == NULL) {

            xTaskCreatePinnedToCore(nivel_luz_ambiente_manejador, 
                                    "task_nivel_luz", 
                                    10000, &tarea_nivel_luz_ambiente, 
                                    2, NULL, 1);

            xTaskCreatePinnedToCore(temperatura_ambiente_manejador, 
                                    "task_temperatura_ambiente", 
                                    10000, &tarea_temperatura_ambiente, 
                                    2, NULL, 0);
            
            estado_termostato = TERMOSTATO_ENCENDIDO;

        }

    } 
    vTaskDelete(NULL);
}

/*Funcion Apagar Termostato*/
static void apagar_termostato_manejador(void * pvParameters) {
    if(estado_termostato == TERMOSTATO_ENCENDIDO) {
        vTaskSuspend(tarea_temperatura_ambiente);
        vTaskSuspend(tarea_nivel_luz_ambiente);

        display.clearDisplay();
        display.setTextSize(1);
        display.setTextColor(WHITE);
        display.setCursor(1, 0);
        display.println(F("Termostato Apagado"));
        display.display();
        Serial.println("Termostato Apagado");

        estado_termostato = TERMOSTATO_APAGADO;
    }
    vTaskDelete(NULL);
}

static void IRAM_ATTR accion_apagar_boton() {
    /*Creacion de la Tarea para Apagar el Termostato*/
    if(tarea_apagar_termostato == NULL) {
        xTaskCreatePinnedToCore(apagar_termostato_manejador, 
                                "task_apagar_termostato", 
                                10000, &tarea_apagar_termostato, 
                                3, NULL, 0);
    }
}

static void IRAM_ATTR accion_encender_boton() {
    /*Creacion de la Tarea para Encender el Termostato*/
    if(tarea_encender_termostato == NULL) {
        xTaskCreatePinnedToCore(encender_termostato_manejador, 
                                "task_encender_termostato", 
                                10000, &tarea_encender_termostato, 
                                3, NULL, 0);
    }
}

void initPlaca() {

    //Para que de tiempo a conectar y a mostrar el Serial en VS
    delay(4000);

    //Serial para mostrar mensajes en el monitor para debuggear o si falla algo
    Serial.begin(115200);

    //Apagado LED RGB (se activa una vez desplegado el programa en el microcontrolador)
    pinMode(R, OUTPUT);
    pinMode(G, OUTPUT);
    pinMode(B, OUTPUT);
    digitalWrite(R,0);
    digitalWrite(G,0);
    digitalWrite(B,0);

    Serial.println("Probando conexion de Pantalla OLED.....");

    if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { 
        Serial.println(F("Fallo al Conectar la Pantalla OLED"));
        for(;;);
    }

    Serial.println("Pantalla OLED conectada");

    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(1, 0);
    display.println(F("Termostato Apagado"));
    display.display();
    Serial.println("Termostato Apagado");
    estado_termostato = TERMOSTATO_APAGADO;

    //Se establece el sensor de temperatura
    dht.begin();

    //Indicamos los pines que son de entrada y de salida
    pinMode(sensorLuz, INPUT);
    pinMode(potentiometer, INPUT);
    pinMode(led2, OUTPUT);
    digitalWrite(led2,0);

    //Se establecen las interrupciones para encender o apagar el termostato
    pinMode(switch1, INPUT);
    pinMode(switch2, INPUT);
    attachInterrupt(digitalPinToInterrupt(switch1), &accion_encender_boton, FALLING);
    attachInterrupt(digitalPinToInterrupt(switch2), &accion_apagar_boton, FALLING);
    //MODO FALLING: HIGH -> LOW (SE PULSA UN BOTON)

}

void setup() {
    initPlaca();
}

void loop() {
}
